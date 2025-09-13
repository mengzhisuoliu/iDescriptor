#include "../../iDescriptor.h"
#include "libirecovery.h"
#include <QDebug>
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <pugixml.hpp>

std::string safeGet(const char *key, pugi::xml_node dict)
{
    for (pugi::xml_node child = dict.first_child(); child;
         child = child.next_sibling()) {
        if (strcmp(child.name(), "key") == 0 &&
            strcmp(child.text().as_string(), key) == 0) {
            pugi::xml_node value = child.next_sibling();
            if (value)
                return value.text().as_string();
        }
    }
    return "";
};

// TODO: return tyype
DeviceInfo fullDeviceInfo(const pugi::xml_document &doc,
                          afc_client_t &afcClient, plist_t &diagnostics,
                          DeviceInfo &d)
{
    pugi::xml_node dict = doc.child("plist").child("dict");
    auto safeGet = [&](const char *key) -> std::string {
        for (pugi::xml_node child = dict.first_child(); child;
             child = child.next_sibling()) {
            if (strcmp(child.name(), "key") == 0 &&
                strcmp(child.text().as_string(), key) == 0) {
                pugi::xml_node value = child.next_sibling();
                if (value)
                    return value.text().as_string();
            }
        }
        return "";
    };

    auto safeGetBool = [&](const char *key) -> bool {
        for (pugi::xml_node child = dict.first_child(); child;
             child = child.next_sibling()) {
            if (strcmp(child.name(), "key") == 0 &&
                strcmp(child.text().as_string(), key) == 0) {
                pugi::xml_node value = child.next_sibling();
                if (value && strcmp(value.name(), "true") == 0)
                    return true;
                else
                    return false;
            }
        }
        return false;
    };

    d.deviceName = safeGet("DeviceName");
    d.deviceClass = safeGet("DeviceClass");
    d.deviceColor = safeGet("DeviceColor");
    d.modelNumber = safeGet("ModelNumber");
    d.cpuArchitecture = safeGet("CPUArchitecture");
    d.buildVersion = safeGet("BuildVersion");
    d.hardwareModel = safeGet("HardwareModel");
    d.hardwarePlatform = safeGet("HardwarePlatform");
    d.ethernetAddress = safeGet("EthernetAddress");
    d.bluetoothAddress = safeGet("BluetoothAddress");
    d.firmwareVersion = safeGet("FirmwareVersion");
    d.productVersion = safeGet("ProductVersion");

    /*DiskInfo*/
    try {
        d.diskInfo.totalDiskCapacity =
            std::stoull(safeGet("TotalDiskCapacity"));
        d.diskInfo.totalDataCapacity =
            std::stoull(safeGet("TotalDataCapacity"));
        d.diskInfo.totalSystemCapacity =
            std::stoull(safeGet("TotalSystemCapacity"));
        d.diskInfo.totalDataAvailable =
            std::stoull(safeGet("TotalDataAvailable"));
    } catch (const std::exception &e) {
        qDebug() << e.what();
        /*It's ok if any of those fails*/
    }

    std::string _activationState = safeGet("ActivationState");

    /* older devices dont have fusing status lets default to ProductionSOC for
     * now*/
    // std::string fStatus = safeGet("FusingStatus");
    // d.productionDevice = std::stoi(fStatus.empty() ? "0" : fStatus) == 3;

    d.productionDevice = safeGetBool("ProductionSOC");
    if (_activationState == "Activated") {
        d.activationState = DeviceInfo::ActivationState::Activated;
    } else if (_activationState == "FactoryActivated") {
        d.activationState = DeviceInfo::ActivationState::FactoryActivated;
    } else if (_activationState == "Unactivated") {
        d.activationState = DeviceInfo::ActivationState::Unactivated;
    } else {
        d.activationState =
            DeviceInfo::ActivationState::Unactivated; // Default value
    }
    // TODO:RegionInfo: LL/A
    d.productType = parse_product_type(safeGet("ProductType"));
    d.jailbroken = detect_jailbroken(afcClient);

    uint64_t cycleCount;
    plist_get_uint_val(
        PlistNavigator(diagnostics)["IORegistry"]["BatteryData"]["CycleCount"],
        &cycleCount);

    char *batterySerialNumber = nullptr;
    plist_get_string_val(
        PlistNavigator(
            diagnostics)["IORegistry"]["BatteryData"]["BatterySerialNumber"],
        &batterySerialNumber);

    uint64_t designCapacity = 0;
    plist_get_uint_val(
        PlistNavigator(
            diagnostics)["IORegistry"]["BatteryData"]["DesignCapacity"],
        &designCapacity);

    uint64_t absoluteCapacity = 0;
    plist_get_uint_val(
        PlistNavigator(diagnostics)["IORegistry"]["AbsoluteCapacity"],
        &absoluteCapacity);

    d.batteryInfo.health =
        QString::number((absoluteCapacity * 100) / designCapacity) + "%";
    d.batteryInfo.cycleCount = cycleCount;
    d.batteryInfo.serialNumber = batterySerialNumber
                                     ? batterySerialNumber
                                     : "Error retrieving serial number";

    plist_free(diagnostics);
    diagnostics = nullptr;

    return d;
}

// TODO: need to handle errors and free resources properly
IDescriptorInitDeviceResult init_idescriptor_device(const char *udid)
{
    // TODO:on a broken usb cable this can hang for a long time
    // causing the UI to freeze
    qDebug() << "Initializing iDescriptor device with UDID: "
             << QString::fromUtf8(udid);
    IDescriptorInitDeviceResult result = {};

    lockdownd_client_t client;
    // TODO: LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING
    // LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING         = -19,
    lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
    lockdownd_service_descriptor_t lockdownService = nullptr;
    diagnostics_relay_client_t diagnostics_client = nullptr;
    afc_client_t afcClient = nullptr;
    try {
        idevice_error_t ret = idevice_new_with_options(&result.device, udid,
                                                       IDEVICE_LOOKUP_USBMUX);

        if (ret != IDEVICE_E_SUCCESS) {
            qDebug() << "Failed to connect to device: " << ret;
            return result;
        }
        if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(
                                       result.device, &client, APP_LABEL))) {
            result.error = ldret;
            qDebug() << "Failed to create lockdown client: " << ldret;
            idevice_free(result.device);
            return result;
        }

        if (LOCKDOWN_E_SUCCESS !=
            (ldret = lockdownd_start_service(client, "com.apple.afc",
                                             &lockdownService))) {
            lockdownd_client_free(client);
            idevice_free(result.device);
            qDebug() << "Failed to start AFC service: " << ldret;
            return result;
        }
        if (lockdownService) {
            qDebug() << "AFC service started successfully.";
        } else {
            qDebug() << "AFC service descriptor is null.";
            // lockdownd_client_free(result.client);
            // idevice_free(result.device);
            // return result;
        }

        if (afc_client_new(result.device, lockdownService, &afcClient) !=
            AFC_E_SUCCESS) {
            lockdownd_service_descriptor_free(lockdownService);
            lockdownd_client_free(client);
            idevice_free(result.device);
            qDebug() << "Failed to create AFC client: " << ldret;
            return result;
        }

        if (diagnostics_relay_client_start_service(
                result.device, &diagnostics_client, nullptr) !=
            DIAGNOSTICS_RELAY_E_SUCCESS) {
            qDebug() << "Failed to start diagnostics relay service.";
            return result;
        }

        pugi::xml_document infoXml;
        get_device_info_xml(udid, 0, 0, infoXml, client, result.device);

        if (infoXml.empty()) {
            qDebug() << "Failed to retrieve device info XML for UDID: "
                     << QString::fromUtf8(udid);
            // Clean up resources before returning
            // afc_client_free(result.afcClient);
            // lockdownd_service_descriptor_free(result.lockdownService);
            // lockdownd_client_free(result.client);
            idevice_free(result.device);
            return result;
        }

        plist_t diagnostics = nullptr;
        std::string productType =
            safeGet("ProductType", infoXml.child("plist").child("dict"));

        bool is_iphone =
            safeGet("DeviceClass", infoXml.child("plist").child("dict")) ==
            "iPhone";
        if (is_iphone) {

            qDebug() << "iPhone is newer than iPhone 8 ?"
                     << is_product_type_newer(productType,
                                              std::string("iPhone10,1"));
        }

        const char *batteryQuery =
            is_iphone
                ? is_product_type_newer(productType, std::string("iPhone8,1"))
                      ? "AppleSmartBattery"
                      : "AppleARMPMUCharger"
                : "AppleARMPMUCharger";
        // TODO: iPhone 8 and above should query AppleSmartBattery
        // TODO: try catch here

        if (diagnostics_relay_query_ioregistry_entry(
                diagnostics_client, nullptr, batteryQuery, &diagnostics) !=
                DIAGNOSTICS_RELAY_E_SUCCESS &&
            !diagnostics) {

            qDebug()
                << "Failed to query diagnostics relay for AppleARMPMUCharger.";
            // Clean up resources before returning
            // if (afcClient)
            // afc_client_free(afcClient);
            if (lockdownService)
                lockdownd_service_descriptor_free(lockdownService);
            if (client)
                lockdownd_client_free(client);
            if (diagnostics_client)
                diagnostics_relay_client_free(diagnostics_client);
            return result;
        }

        // if (result.device) idevice_free(result.device);

        fullDeviceInfo(infoXml, afcClient, diagnostics, result.deviceInfo);
        result.afcClient = afcClient;
        result.success = true;
        // TODO: cleanup needed ?
        // if (afcClient)
        //     afc_client_free(afcClient);
        // if (lockdownService)
        //     lockdownd_service_descriptor_free(lockdownService);
        // if (client)
        //     lockdownd_client_free(client);
        // if (diagnostics_client)
        //     diagnostics_relay_client_free(diagnostics_client);
        return result;

    } catch (const std::exception &e) {
        qDebug() << "Exception in init_idescriptor_device: " << e.what();
        // Clean up any allocated resources
        // if (result.afcClient) afc_client_free(result.afcClient);
        // if (result.lockdownService)
        // lockdownd_service_descriptor_free(result.lockdownService);
        if (client)
            lockdownd_client_free(client);
        if (result.device)
            idevice_free(result.device);
        return result;
    }
}

IDescriptorInitDeviceResultRecovery
init_idescriptor_recovery_device(irecv_device_info *info)
{
    IDescriptorInitDeviceResultRecovery result;
    result.deviceInfo = *info;
    uint64_t ecid = info->ecid;
    // irecv_client_t client = nullptr;
    // Docs say that clients are not long-lived, so instead of storing, we
    // create a new one each time we need it. irecv_error_t ret =
    // irecv_open_with_ecid_and_attempts(&client, ecid,
    // RECOVERY_CLIENT_CONNECTION_TRIES);

    // if (ret != IRECV_E_SUCCESS)
    // {
    //     return result;
    // }

    result.success = true;
    return result;
}