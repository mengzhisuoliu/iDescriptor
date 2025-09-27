#include "../../iDescriptor.h"
#include "libirecovery.h"
#include <QDebug>
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

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

    qDebug() << "appleRawCurrentCapacity" << appleRawCurrentCapacity;
    qDebug() << "appleRawMaxCapacity" << appleRawMaxCapacity;

    uint64_t oldCurrrentBatteryLevel =
        (appleRawCurrentCapacity && appleRawMaxCapacity)
            ? (appleRawCurrentCapacity * 100 / appleRawMaxCapacity)
            : 0;
    qDebug() << "oldCurrrentBatteryLevel" << oldCurrrentBatteryLevel;

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

    d.batteryInfo.currentBatteryLevel =
        ioreg["BatteryData"]["StateOfCharge"].getUInt();

    d.batteryInfo.usbConnectionType =
        ioreg["AdapterDetails"]["Description"].getString() == "usb type-c"
            ? BatteryInfo::ConnectionType::USB_TYPEC
            : BatteryInfo::ConnectionType::USB;

    // adaptor details
    d.batteryInfo.adapterVoltage =
        ioreg["AppleRawAdapterDetails"][0]["AdapterVoltage"].getUInt();

    d.batteryInfo.watts = ioreg["AppleRawAdapterDetails"][0]["Watts"].getUInt();
}

// TODO: return tyype
DeviceInfo fullDeviceInfo(const pugi::xml_document &doc,
                          afc_client_t &afcClient,
                          IDescriptorInitDeviceResult &result)
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
    // TODO:RegionInfo: LL/A
    std::string rawProductType = safeGet("ProductType");
    d.productType = parse_product_type(rawProductType);
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

// TODO: IDescriptorInitDeviceResult
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
    afc_client_t afcClient = nullptr;
    afc_client_t afc2Client = nullptr;
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

        try {
            afc_error_t err = AFC_E_UNKNOWN_ERROR;
            if ((err = afc2_client_new(result.device, &afc2Client)) !=
                AFC_E_SUCCESS) {
                qDebug() << "AFC2 client not available." << "Error:" << err;
            } else {
                result.afc2Client = afc2Client;
                qDebug() << "AFC2 client created successfully.";
            }
        } catch (const std::exception &e) {
            /* Fine! This only works on Jailbroken and AFC2 tweak installed
             * devices */
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

        std::string productType =
            safeGetXML("ProductType", infoXml.child("plist").child("dict"));

        // if (result.device) idevice_free(result.device);

        fullDeviceInfo(infoXml, afcClient, result);
        result.afcClient = afcClient;
        result.success = true;

        if (lockdownService)
            lockdownd_service_descriptor_free(lockdownService);
        if (client)
            lockdownd_client_free(client);

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