#include "../../devicedatabase.h"
#include "../../iDescriptor.h"
#include "../../servicemanager.h"
#include "libirecovery.h"
#include <QDebug>
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <string.h>

std::string safeGetXML(const char *key, pugi::xml_node dict)
{
    for (pugi::xml_node child = dict.first_child(); child;
         child = child.next_sibling()) {
        if (strcmp(child.name(), "key") == 0 &&
            strcmp(child.text().as_string(), key) == 0) {
            pugi::xml_node value = child.next_sibling();
            if (value) {
                // Handle different XML element types
                if (strcmp(value.name(), "true") == 0) {
                    return "true";
                } else if (strcmp(value.name(), "false") == 0) {
                    return "false";
                } else if (strcmp(value.name(), "integer") == 0) {
                    return value.text().as_string();
                } else if (strcmp(value.name(), "string") == 0) {
                    return value.text().as_string();
                } else if (strcmp(value.name(), "real") == 0) {
                    return value.text().as_string();
                } else {
                    // For any other type, try to get the text content
                    return value.text().as_string();
                }
            }
        }
    }
    return "";
}

// this is reused in the ui in deviceinfowidget
void parseOldDeviceBattery(PlistNavigator &ioreg, DeviceInfo &d)
{
    d.batteryInfo.isCharging = ioreg["IsCharging"].getBool();

    d.batteryInfo.fullyCharged = ioreg["FullyCharged"].getBool();

    uint64_t appleRawCurrentCapacity =
        ioreg["AppleRawCurrentCapacity"].getUInt();
    uint64_t appleRawMaxCapacity = ioreg["AppleRawMaxCapacity"].getUInt();

    uint64_t oldCurrrentBatteryLevel =
        (appleRawCurrentCapacity && appleRawMaxCapacity)
            ? (appleRawCurrentCapacity * 100 / appleRawMaxCapacity)
            : 0;

    d.batteryInfo.currentBatteryLevel = oldCurrrentBatteryLevel;

    // adaptor details
    d.batteryInfo.usbConnectionType =
        ioreg["AdapterDetails"]["Description"].getString() == "usb type-c"
            ? BatteryInfo::ConnectionType::USB_TYPEC
            : BatteryInfo::ConnectionType::USB;
    d.batteryInfo.adapterVoltage = 0;

    // watt
    d.batteryInfo.watts = ioreg["AdapterDetails"]["Watts"].getUInt();
}

void parseOldDevice(PlistNavigator &ioreg, DeviceInfo &d)
{
    uint64_t cycleCount = ioreg["CycleCount"].getUInt();

    // skipping on very old devices for now
    std::string batterySerialNumber = "";
    uint64_t designCapacity = ioreg["DesignCapacity"].getUInt();

    uint64_t maxCapacity = ioreg["MaxCapacity"].getUInt();

    qDebug() << "Design capacity: " << designCapacity;
    qDebug() << "Max capacity: " << maxCapacity;

    // Compat
    int healthPercent =
        (designCapacity != 0) ? (maxCapacity * 100) / designCapacity : 0;
    healthPercent = std::min(healthPercent, 100);
    d.batteryInfo.health = QString::number(healthPercent) + "%";
    d.batteryInfo.cycleCount = cycleCount;
    d.batteryInfo.serialNumber = !batterySerialNumber.empty()
                                     ? batterySerialNumber
                                     : "Error retrieving serial number";

    parseOldDeviceBattery(ioreg, d);
}

// this is reused in the ui in deviceinfowidget
void parseDeviceBattery(PlistNavigator &ioreg, DeviceInfo &d)
{
    d.batteryInfo.isCharging = ioreg["IsCharging"].getBool();

    d.batteryInfo.fullyCharged = ioreg["FullyCharged"].getBool();

    qDebug() << "Stalebatteryinfo:"
             << ioreg["BatteryData"]["StateOfCharge"].getUInt();
    /* data is stale here so we need to calculate */
    // d.batteryInfo.currentBatteryLevel =
    //     ioreg["BatteryData"]["StateOfCharge"].getUInt();

    uint64_t appleRawCurrentCapacity =
        ioreg["AppleRawCurrentCapacity"].getUInt();
    uint64_t appleRawMaxCapacity = ioreg["AppleRawMaxCapacity"].getUInt();

    uint64_t currentBatteryLevel =
        (appleRawCurrentCapacity && appleRawMaxCapacity)
            ? (appleRawCurrentCapacity * 100 / appleRawMaxCapacity)
            : 0;

    d.batteryInfo.currentBatteryLevel = currentBatteryLevel;

    d.batteryInfo.usbConnectionType =
        ioreg["AdapterDetails"]["Description"].getString() == "usb type-c"
            ? BatteryInfo::ConnectionType::USB_TYPEC
            : BatteryInfo::ConnectionType::USB;

    // adaptor details
    d.batteryInfo.adapterVoltage =
        ioreg["AppleRawAdapterDetails"][0]["AdapterVoltage"].getUInt();

    d.batteryInfo.watts = ioreg["AppleRawAdapterDetails"][0]["Watts"].getUInt();
}

DeviceInfo fullDeviceInfo(const pugi::xml_document &doc,
                          afc_client_t &afcClient,
                          iDescriptorInitDeviceResult &result)
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
    DeviceInfo &d = result.deviceInfo;
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

    QString q_version = QString::fromStdString(d.productVersion);
    QStringList parts = q_version.split('.');

    int major = (parts.length() > 0) ? parts[0].toInt() : 0;
    int minor = (parts.length() > 1) ? parts[1].toInt() : 0;
    int patch = (parts.length() > 2) ? parts[2].toInt() : 0;

    d.parsedDeviceVersion = IDESCRIPTOR_DEVICE_VERSION(major, minor, patch);

    /*DiskInfo*/
    try {
        d.diskInfo.totalDiskCapacity =
            std::stoull(safeGet("TotalDiskCapacity"));
        d.diskInfo.totalDataCapacity =
            std::stoull(safeGet("TotalDataCapacity"));
        d.diskInfo.totalSystemCapacity =
            std::stoull(safeGet("TotalSystemCapacity"));
        /*
            For some reason this is way inaccrutate for iOS 17 and up
        */
        d.diskInfo.totalDataAvailable =
            std::stoull(safeGet("TotalDataAvailable"));

        try {
            /*
                Example : this data seems to be the most accurate
            */
            //"Model: iPhone12,8"
            // "FSTotalBytes: 63966400512"
            // "FSFreeBytes: 2867101696"
            // "FSBlockSize: 4096"
            char **info = NULL;
            afc_get_device_info(afcClient, &info);
            if (info && info[6]) {
                d.diskInfo.totalDataAvailable =
                    std::stoull(std::string(info[5]));
            }
            afc_dictionary_free(info);
        } catch (const std::exception &e) {
            qDebug() << "Error parsing disk info: " << e.what();
        }
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
        // IOS 6
    } else if (_activationState == "WildcardActivated") {
        d.activationState =
            DeviceInfo::ActivationState::Activated; // Treat as activated
    } else if (_activationState == "FactoryActivated") {
        d.activationState = DeviceInfo::ActivationState::FactoryActivated;
    } else if (_activationState == "Unactivated") {
        d.activationState = DeviceInfo::ActivationState::Unactivated;
    } else {
        d.activationState =
            DeviceInfo::ActivationState::Unactivated; // Default value
    }
    std::string regionInfo = safeGet("RegionInfo");
    d.regionRaw = regionInfo;
    d.region = DeviceDatabase::parseRegionInfo(regionInfo);
    std::string rawProductType = safeGet("ProductType");
    const DeviceDatabaseInfo *info =
        DeviceDatabase::findByIdentifier(rawProductType);
    d.productType =
        info ? info->displayName ? info->displayName : info->marketingName
             : "Unknown Device";
    d.marketingName = info ? info->marketingName : "Unknown Device";
    d.rawProductType = rawProductType;
    d.jailbroken = detect_jailbroken(afcClient);
    d.is_iPhone = safeGet("DeviceClass") == "iPhone";

    /*BatteryInfo*/
    plist_t diagnostics = nullptr;
    get_battery_info(rawProductType, result.device, d.is_iPhone, diagnostics);

    if (!diagnostics) {
        qDebug() << "Failed to get diagnostics plist.";
        return d;
    }
    try {
        PlistNavigator ioreg = PlistNavigator(diagnostics)["IORegistry"];

        // old devices do not have "BatteryData"
        d.oldDevice = !ioreg["BatteryData"];
        if (d.oldDevice) {
            parseOldDevice(ioreg, d);
            plist_free(diagnostics);
            diagnostics = nullptr;
            return d;
        }

        bool newerThaniPhone8 =
            is_product_type_newer(rawProductType, std::string("iPhone8,1"));

        uint64_t cycleCount = ioreg["BatteryData"]["CycleCount"].getUInt();

        // Battery serial number
        std::string batterySerialNumber =
            ioreg["BatteryData"]["BatterySerialNumber"].getString();

        uint64_t designCapacity =
            ioreg["BatteryData"]["DesignCapacity"].getUInt();

        uint64_t maxCapacity =
            d.is_iPhone ? newerThaniPhone8
                              ? ioreg["AppleRawMaxCapacity"].getUInt()
                              : ioreg["BatteryData"]["MaxCapacity"].getUInt()
                        : ioreg["BatteryData"]["MaxCapacity"].getUInt();

        qDebug() << "Design capacity: " << designCapacity;
        qDebug() << "Max capacity: " << maxCapacity;

        // seems to be to the most accurate way to get health
        d.batteryInfo.health =
            QString::number((maxCapacity * 100) / designCapacity) + "%";
        d.batteryInfo.cycleCount = cycleCount;
        d.batteryInfo.serialNumber = !batterySerialNumber.empty()
                                         ? batterySerialNumber
                                         : "Error retrieving serial number";
        parseDeviceBattery(ioreg, d);
        plist_free(diagnostics);
        diagnostics = nullptr;

        return d;
    } catch (const std::exception &e) {
        qDebug() << "Error occurred: " << e.what();
        return d;
    }
}

iDescriptorInitDeviceResult init_idescriptor_device(const char *udid)
{
    qDebug() << "Initializing iDescriptor device with UDID: "
             << QString::fromUtf8(udid);
    iDescriptorInitDeviceResult result = {};

    // 1. Initialize all resource handles to nullptr
    idevice_t device = nullptr;
    lockdownd_client_t client = nullptr;
    lockdownd_service_descriptor_t lockdownService = nullptr;
    afc_client_t afcClient = nullptr;
    afc_client_t afc2Client = nullptr;
    pugi::xml_document infoXml;

    idevice_error_t ret =
        idevice_new_with_options(&device, udid, IDEVICE_LOOKUP_USBMUX);

    if (ret != IDEVICE_E_SUCCESS) {
        qDebug() << "Failed to connect to device: " << ret;
        // result.error is not set here as idevice_error_t is different
        goto cleanup;
    }

    lockdownd_error_t ldret;
    if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(
                                   device, &client, APP_LABEL))) {
        result.error = ldret;
        qDebug() << "Failed to create lockdown client: " << ldret;
        goto cleanup;
    }

    if (LOCKDOWN_E_SUCCESS !=
        (ldret = lockdownd_start_service(client, "com.apple.afc",
                                         &lockdownService))) {
        result.error = ldret;
        qDebug() << "Failed to start AFC service: " << ldret;
        goto cleanup;
    }

    if (afc_client_new(device, lockdownService, &afcClient) != AFC_E_SUCCESS) {
        qDebug() << "Failed to create AFC client.";

        goto cleanup;
    }

    // AFC2 is optional, so we don't goto cleanup on failure
    afc_error_t afc2_err;
    if ((afc2_err = afc2_client_new(device, &afc2Client)) != AFC_E_SUCCESS) {
        qDebug() << "AFC2 client not available. Error:" << afc2_err;
        afc2Client = nullptr;
    } else {
        qDebug() << "AFC2 client created successfully.";
    }

    get_device_info_xml(udid, client, device, infoXml);

    if (infoXml.empty()) {
        qDebug() << "Failed to retrieve device info XML for UDID: "
                 << QString::fromUtf8(udid);
        goto cleanup;
    }

    // If we got this far, the core initialization is successful
    result.success = true;
    result.device = device;
    result.afcClient = afcClient;
    result.afc2Client = afc2Client;
    fullDeviceInfo(infoXml, afcClient, result);

cleanup:
    if (lockdownService) {
        lockdownd_service_descriptor_free(lockdownService);
    }
    if (client) {
        lockdownd_client_free(client);
    }

    // free on error
    if (!result.success) {
        if (afc2Client) {
            afc_client_free(afc2Client);
        }
        if (afcClient) {
            afc_client_free(afcClient);
        }
        if (device) {
            idevice_free(device);
        }
    }

    return result;
}

iDescriptorInitDeviceResultRecovery
init_idescriptor_recovery_device(uint64_t ecid)
{
    qDebug() << "Initializing iDescriptor recovery device with ECID: " << ecid;
    iDescriptorInitDeviceResultRecovery result = {};

    irecv_client_t client = nullptr;
    const irecv_device_info *deviceInfo = nullptr;
    irecv_device_t device = nullptr;
    const DeviceDatabaseInfo *info = nullptr;

    irecv_error_t ret = irecv_open_with_ecid_and_attempts(
        &client, ecid, RECOVERY_CLIENT_CONNECTION_TRIES);

    if (ret != IRECV_E_SUCCESS) {
        qDebug() << "Failed to open recovery client with ECID:" << ecid
                 << "Error:" << ret;
        result.error = ret;
        goto cleanup;
    }

    ret = irecv_get_mode(client, (int *)&result.mode);
    if (ret != IRECV_E_SUCCESS) {
        qDebug() << "Failed to get recovery mode. Error:" << ret;
        result.error = ret;
        goto cleanup;
    }

    deviceInfo = irecv_get_device_info(client);
    if (!deviceInfo) {
        qDebug() << "Failed to get device info from recovery client";
        result.error = IRECV_E_UNKNOWN_ERROR;
        goto cleanup;
    }

    if (irecv_devices_get_device_by_client(client, &device) ==
            IRECV_E_SUCCESS &&
        device && device->hardware_model) {
        qDebug() << "Recovery device hardware_model: "
                 << device->hardware_model;
        info =
            DeviceDatabase::findByHwModel(std::string(device->hardware_model));
    } else {
        qDebug() << "Could not resolve hardware_model from client.";
    }

    result.displayName =
        info ? (info->displayName ? info->displayName : info->marketingName)
             : "Unknown Device";
    result.deviceInfo = *deviceInfo;
    result.success = true;

cleanup:
    if (client) {
        irecv_close(client);
    }

    return result;
}