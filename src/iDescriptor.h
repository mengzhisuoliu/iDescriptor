#pragma once
#include <QDebug>
#include <QImage>
#include <QtCore/QObject>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/mobile_image_mounter.h>
#include <libimobiledevice/screenshotr.h>
#include <libirecovery.h>
#include <mutex>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#define TOOL_NAME "iDescriptor"
#define APP_LABEL "iDescriptor"
#define APP_VERSION "0.1.0"
#define APP_COPYRIGHT "© 2025 Uncore. All rights reserved."
#define AFC2_SERVICE_NAME "com.apple.afc2"
#define RECOVERY_CLIENT_CONNECTION_TRIES 3
#define APPLE_VENDOR_ID 0x05ac

// This is because afc_read_directory accepts  "/var/mobile/Media" as "/"
#define POSSIBLE_ROOT "../../../../"

struct BatteryInfo {
    QString health;
    uint64_t cycleCount;
    // uint64_t designCapacity;
    // uint64_t maxCapacity;
    // uint64_t fullChargeCapacity;
    std::string serialNumber;
    bool isCharging;
    bool fullyCharged;
    uint64_t currentBatteryLevel;
    enum class ConnectionType {
        USB,
        USB_TYPEC,
    } usbConnectionType;
    uint64_t adapterVoltage; // in mV
    uint64_t watts;
};

//! IOS 12
/* {
"AmountDataAvailable": 6663077888,
"AmountDataReserved": 209715200,
"AmountRestoreAvailable": 11524079616,
"CalculateDiskUsage": "OkilyDokily",
"NANDInfo": <01000000 01000000 01000000 00000080 ... 00 00000000 000000>,
"TotalDataAvailable": 6872793088,
"TotalDataCapacity": 11306721280,
"TotalDiskCapacity": 16000000000,
"TotalSystemAvailable": 0,
"TotalSystemCapacity": 4693204992
}*/
struct DiskInfo {
    uint64_t totalDiskCapacity;
    uint64_t totalDataCapacity;
    uint64_t totalSystemCapacity;
    uint64_t totalDataAvailable;
};

struct DeviceInfo {
    enum class ActivationState {
        Activated,
        FactoryActivated,
        Unactivated
    } activationState;
    std::string activationStateAcknowledged;
    std::string productType;
    std::string rawProductType;
    bool jailbroken;
    std::string basebandActivationTicketVersion;
    std::string basebandCertId;
    std::string basebandChipID;
    std::string basebandKeyHashInformation;
    std::string aKeyStatus;
    std::string sKeyHash;
    std::string sKeyStatus;
    std::string basebandMasterKeyHash;
    std::string basebandRegionSKU;
    std::string basebandSerialNumber;
    std::string basebandStatus;
    std::string basebandVersion;
    std::string bluetoothAddress;
    std::string boardId;
    std::string productVersion;
    bool brickState;
    std::string buildVersion;
    std::string cpuArchitecture;
    std::string carrierBundleInfoArray_1;
    std::string cfBundleIdentifier;
    std::string cfBundleVersion;
    std::string gid1;
    std::string gid2;
    std::string integratedCircuitCardIdentity;
    std::string internationalMobileSubscriberIdentity;
    std::string mcc;
    std::string mnc;
    std::string mobileEquipmentIdentifier;
    std::string simGid1;
    std::string simGid2;
    std::string slot;
    std::string kCTPostponementInfoAvailable;
    std::string certID;
    std::string chipID;
    std::string chipSerialNo;
    std::string deviceClass;
    std::string deviceColor;
    std::string deviceName;
    std::string dieID;
    std::string ethernetAddress;
    std::string firmwareVersion;
    int fusingStatus;
    std::string hardwareModel;
    std::string hardwarePlatform;
    bool hasSiDP;
    bool hostAttached;
    std::string internationalMobileEquipmentIdentity;
    bool internationalMobileSubscriberIdentityOverride;
    std::string mlbSerialNumber;
    std::string mobileSubscriberCountryCode;
    std::string mobileSubscriberNetworkCode;
    std::string modelNumber;
    // NonVolatileRAM omitted (unknown type)
    std::string ioNVRAMSyncNowProperty;
    bool systemAudioVolumeSaved;
    bool autoBoot;
    int backlightLevel;
    bool productionDevice;
    BatteryInfo batteryInfo;
    DiskInfo diskInfo;
    bool is_iPhone;
    bool oldDevice;
    std::string marketingName;
    std::string regionRaw;
    std::string region;
    unsigned int parsedDeviceVersion;
};

struct iDescriptorDevice {
    std::string udid;
    idevice_connection_type conn_type;
    idevice_t device;
    DeviceInfo deviceInfo;
    afc_client_t afcClient;
    afc_client_t afc2Client;
    bool is_iPhone;
    std::recursive_mutex *mutex;
};

struct iDescriptorInitDeviceResult {
    bool success = false;
    lockdownd_error_t error;
    idevice_t device;
    DeviceInfo deviceInfo;
    afc_client_t afcClient;
    afc_client_t afc2Client;
};

struct iDescriptorRecoveryDevice {
    uint64_t ecid;
    irecv_mode mode;
    uint32_t cpid;
    uint32_t bdid;
    std::string displayName;
    std::recursive_mutex *mutex;
};

struct TakeScreenshotResult {
    bool success = false;
    QImage img;
};

struct iDescriptorInitDeviceResultRecovery {
    irecv_client_t client = nullptr;
    irecv_device_info deviceInfo;
    irecv_error_t error;
    bool success = false;
    irecv_mode mode = IRECV_K_RECOVERY_MODE_1;
    const char *displayName = nullptr;
};

void warn(const QString &message, const QString &title = "Warning",
          QWidget *parent = nullptr);

enum class AddType { Regular, Pairing };

class PlistNavigator
{
private:
    plist_t current_node;

public:
    PlistNavigator(plist_t node) : current_node(node) {}

    // dict key access
    PlistNavigator operator[](const char *key)
    {
        if (!current_node || plist_get_node_type(current_node) != PLIST_DICT) {
            return PlistNavigator(nullptr);
        }
        plist_t next = plist_dict_get_item(current_node, key);
        return PlistNavigator(next);
    }

    // array index access
    PlistNavigator operator[](int index)
    {
        if (!current_node || plist_get_node_type(current_node) != PLIST_ARRAY) {
            return PlistNavigator(nullptr);
        }
        if (index < 0 ||
            index >= static_cast<int>(plist_array_get_size(current_node))) {
            return PlistNavigator(nullptr);
        }
        plist_t next = plist_array_get_item(current_node, index);
        return PlistNavigator(next);
    }

    operator plist_t() const { return current_node; }
    bool valid() const { return current_node != nullptr; }

    bool getBool() const
    {
        if (!current_node)
            return false;
        uint8_t value = false;
        plist_get_bool_val(current_node, &value);
        return value;
    }

    uint64_t getUInt() const
    {
        if (!current_node)
            return 0;
        uint64_t value = 0;
        plist_get_uint_val(current_node, &value);
        return value;
    }

    std::string getString() const
    {
        if (!current_node)
            return "";
        char *value = nullptr;
        plist_get_string_val(current_node, &value);
        std::string result = value ? value : "";
        if (value)
            free(value);
        return result;
    }
    plist_t getNode() const { return current_node; }
};

afc_error_t safe_afc_read_directory(afc_client_t afcClient, idevice_t device,
                                    const char *path, char ***dirs);

std::string parse_product_type(const std::string &productType);

std::string parse_recovery_mode(irecv_mode productType);

struct MediaEntry {
    std::string name;
    bool isDir;
};

struct AFCFileTree {
    std::vector<MediaEntry> entries;
    bool success;
    std::string currentPath;
};

AFCFileTree get_file_tree(afc_client_t afcClient,
                          const std::string &path = "/");

bool detect_jailbroken(afc_client_t afc);

void get_device_info_xml(const char *udid, lockdownd_client_t client,
                         idevice_t device, pugi::xml_document &infoXml);

iDescriptorInitDeviceResult init_idescriptor_device(const char *udid);

iDescriptorInitDeviceResultRecovery
init_idescriptor_recovery_device(uint64_t ecid);

bool set_location(idevice_t device, char *lat, char *lon);

bool shutdown(idevice_t device);

TakeScreenshotResult take_screenshot(screenshotr_client_t shotr);

mobile_image_mounter_error_t mount_dev_image(idevice_t device,
                                             unsigned int device_version,
                                             const char *image_dir_path);
struct GetMountedImageResult {
    bool success;
    std::string sig;
    std::string message;
};

plist_t _get_mounted_image(const char *udid);

bool restart(std::string udid);

enum class ImageCompatibility {
    Compatible,      // Exact match or known compatible version
    MaybeCompatible, // Major version matches but minor doesn't
    NotCompatible    // Not compatible
};

struct ImageInfo {
    QString version;
    QString dmgPath;
    QString sigPath;
    ImageCompatibility compatibility = ImageCompatibility::NotCompatible;
    bool isDownloaded = false;
    bool isMounted = false;
};

/**
 * @brief Compare two iPhone product types to determine which is newer
 * @param productType First iPhone product type (e.g., "iPhone8,1")
 * @param otherProductType Second iPhone product type (e.g., "iPhone7,2")
 * @return true if productType is newer than otherProductType, false otherwise
 *
 * Examples:
 * - compare_product_type("iPhone8,1", "iPhone7,2") returns true
 * - compare_product_type("iPhone6,1", "iPhone8,1") returns false
 * - compare_product_type("iPhone8,2", "iPhone8,1") returns true
 */
bool compare_product_type(std::string productType,
                          std::string otherProductType);

/**
 * @brief Check if two iPhone product types are exactly equal
 * @param productType First iPhone product type
 * @param otherProductType Second iPhone product type
 * @return true if both product types are identical
 */
bool are_product_types_equal(const std::string &productType,
                             const std::string &otherProductType);

/**
 * @brief Check if first product type is newer than second
 * @param productType First iPhone product type
 * @param otherProductType Second iPhone product type
 * @return true if productType is newer than otherProductType
 */
bool is_product_type_newer(const std::string &productType,
                           const std::string &otherProductType);

/**
 * @brief Check if first product type is older than second
 * @param productType First iPhone product type
 * @param otherProductType Second iPhone product type
 * @return true if productType is older than otherProductType
 */
bool is_product_type_older(const std::string &productType,
                           const std::string &otherProductType);

bool query_mobile_gestalt(iDescriptorDevice *id_device, const QStringList &keys,
                          uint32_t &xml_size, char *&xml_data);
;

std::string safeGetXML(const char *key, pugi::xml_node dict);

void get_battery_info(std::string productType, idevice_t idevice,
                      bool is_iphone, plist_t &diagnostics);

void parseOldDeviceBattery(PlistNavigator &ioreg, DeviceInfo &d);
void parseDeviceBattery(PlistNavigator &ioreg, DeviceInfo &d);

void fetchAppIconFromApple(const QString &bundleId,
                           std::function<void(const QPixmap &)> callback,
                           QObject *context);

afc_error_t afc2_client_new(idevice_t device, afc_client_t *afc);

void get_cable_info(idevice_t device, plist_t &response);

struct NetworkDevice {
    QString name;                           // service name
    QString hostname;                       // e.g., iPhone-2.local
    QString address;                        // IPv4 or IPv6 address
    uint16_t port = 22;                     // SSH port
    std::map<std::string, std::string> txt; // TXT records

    bool operator==(const NetworkDevice &other) const
    {
        return name == other.name && address == other.address;
    }
};

QPixmap load_heic(const QByteArray &data);

QByteArray read_afc_file_to_byte_array(afc_client_t afcClient,
                                       const char *path);

bool isDarkMode();

instproxy_error_t install_IPA(idevice_t device, afc_client_t afc,
                              const char *filePath);

#define IDESCRIPTOR_DEVICE_VERSION(maj, min, patch)                            \
    ((((maj) & 0xFF) << 16) | (((min) & 0xFF) << 8) | ((patch) & 0xFF))

/*
    we need this because idevice_get_device_version
    is not always available in libimobiledevice
    which could cause issues when installed from package managers
*/
inline unsigned int get_device_version(idevice_t _device)
{
    if (!_device) {
        return 0;
    }

    lockdownd_client_t lockdown = NULL;
    if (lockdownd_client_new_with_handshake(
            _device, &lockdown, "iDescriptor") != LOCKDOWN_E_SUCCESS) {
        return 0;
    }

    plist_t node = NULL;
    if (lockdownd_get_value(lockdown, NULL, "ProductVersion", &node) !=
        LOCKDOWN_E_SUCCESS) {
        lockdownd_client_free(lockdown);
        return 0;
    }

    unsigned int version_number = 0;
    if (node && plist_get_node_type(node) == PLIST_STRING) {
        char *version_string = NULL;
        plist_get_string_val(node, &version_string);
        if (version_string) {
            QString q_version = QString(version_string);
            QStringList parts = q_version.split('.');

            int major = (parts.length() > 0) ? parts[0].toInt() : 0;
            int minor = (parts.length() > 1) ? parts[1].toInt() : 0;
            int patch = (parts.length() > 2) ? parts[2].toInt() : 0;

            version_number = IDESCRIPTOR_DEVICE_VERSION(major, minor, patch);

            free(version_string);
        }
    }

    if (node) {
        plist_free(node);
    }
    lockdownd_client_free(lockdown);

    return version_number;
}