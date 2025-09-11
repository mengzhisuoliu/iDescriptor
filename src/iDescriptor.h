#pragma once
#include <QImage>
#include <QtCore/QObject>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/screenshotr.h>
#include <libirecovery.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#define TOOL_NAME "iDescriptor"
#define APP_LABEL "iDescriptor"
#define APP_VERSION "0.0.1"
#define APP_COPYRIGHT "© 2023 Uncore. All rights reserved."

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
};

struct iDescriptorDevice {
    std::string udid;
    idevice_connection_type conn_type;
    idevice_t device;
    DeviceInfo deviceInfo;
    /*
     inital afc client to start the file explorer and gallery with
     clients are not long lived, so do not assume this will be valid
    */
    afc_client_t afcClient;
};

struct IDescriptorInitDeviceResult {
    bool success;
    lockdownd_error_t error;
    idevice_t device;
    DeviceInfo deviceInfo;
    afc_client_t afcClient;
};

// Device model identifier to marketing name mapping
const std::unordered_map<std::string, std::string> DEVICE_MAP = {
    {"iPhone1,1", "iPhone 2G"},
    {"iPhone1,2", "iPhone 3G"},
    {"iPhone2,1", "iPhone 3GS"},
    {"iPhone3,1", "iPhone 4 (GSM)"},
    {"iPhone3,2", "iPhone 4 (GSM Rev A)"},
    {"iPhone3,3", "iPhone 4 (CDMA)"},
    {"iPhone4,1", "iPhone 4S"},
    {"iPhone5,1", "iPhone 5 (GSM)"},
    {"iPhone5,2", "iPhone 5 (GSM+CDMA)"},
    {"iPhone5,3", "iPhone 5c (GSM)"},
    {"iPhone5,4", "iPhone 5c (GSM+CDMA)"},
    {"iPhone6,1", "iPhone 5s (GSM)"},
    {"iPhone6,2", "iPhone 5s (GSM+CDMA)"},
    {"iPhone7,1", "iPhone 6 Plus"},
    {"iPhone7,2", "iPhone 6"},
    {"iPhone8,1", "iPhone 6s"},
    {"iPhone8,2", "iPhone 6s Plus"},
    {"iPhone8,4", "iPhone SE (1st generation)"},
    {"iPhone9,1", "iPhone 7 (GSM)"},
    {"iPhone9,2", "iPhone 7 Plus (GSM)"},
    {"iPhone9,3", "iPhone 7 (GSM+CDMA)"},
    {"iPhone9,4", "iPhone 7 Plus (GSM+CDMA)"},
    {"iPhone10,1", "iPhone 8 (GSM)"},
    {"iPhone10,2", "iPhone 8 Plus (GSM)"},
    {"iPhone10,3", "iPhone X (GSM)"}};

struct RecoveryDeviceInfo : public QObject {
    Q_OBJECT
public:
    RecoveryDeviceInfo(const irecv_device_event_t *event,
                       QObject *parent = nullptr)
        : QObject(parent)
    {
        if (event && event->device_info) {
            ecid = event->device_info->ecid;
            mode = event->mode;
            cpid = event->device_info->cpid;
            bdid = event->device_info->bdid;
        }
    }
    uint64_t ecid;
    irecv_mode mode;
    uint32_t cpid;
    uint32_t bdid;
    QString product;
    QString model;
    QString board_id;
};

struct TakeScreenshotResult {
    bool success;
    QImage img;
};

struct IDescriptorInitDeviceResultRecovery {
    irecv_client_t client = nullptr;
    irecv_device_info deviceInfo;
    bool success = false;
    irecv_mode mode = IRECV_K_RECOVERY_MODE_1;
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

    PlistNavigator operator[](const char *key)
    {
        if (!current_node || plist_get_node_type(current_node) != PLIST_DICT) {
            return PlistNavigator(nullptr);
        }
        plist_t next = plist_dict_get_item(current_node, key);
        return PlistNavigator(next);
    }

    operator plist_t() const { return current_node; }
    bool valid() const { return current_node != nullptr; }
};

afc_error_t safe_afc_read_directory(afc_client_t afcClient, idevice_t device,
                                    const char *path, char ***dirs);

std::string parse_product_type(const std::string &productType);

std::string parse_recovery_mode(irecv_mode productType);

struct MediaEntry {
    std::string name;
    bool isDir;
};

struct MediaFileTree {
    std::vector<MediaEntry> entries;
    bool success;
    std::string currentPath;
};

MediaFileTree get_file_tree(afc_client_t afcClient, idevice_t device,
                            const std::string &path = "/");

bool detect_jailbroken(afc_client_t afc);

void get_device_info_xml(const char *udid, int use_network, int simple,
                         pugi::xml_document &infoXml, lockdownd_client_t client,
                         idevice_t device);

IDescriptorInitDeviceResult init_idescriptor_device(const char *udid);

IDescriptorInitDeviceResultRecovery
init_idescriptor_recovery_device(irecv_device_info *info);

bool set_location(idevice_t device, char *lat, char *lon);

bool shutdown(idevice_t device);

TakeScreenshotResult take_screenshot(screenshotr_client_t shotr);

bool mount_dev_image(const char *udid, const char *image_dir_path);

struct GetMountedImageResult {
    bool success;
    std::string output;
    std::string message;
};

QPair<bool, plist_t> _get_mounted_image(const char *udid);

bool restart(idevice_t device);

// TODO:move
struct ImageInfo {
    QString version;
    QString dmgPath;
    QString sigPath;
    bool isCompatible = false;
    bool isDownloaded = false;
    bool isMounted = false;
};

struct GetImagesSortedResult {
    QStringList compatibleImages;
    QStringList otherImages;
};

struct GetImagesSortedFinalResult {
    QList<ImageInfo> compatibleImages;
    QList<ImageInfo> otherImages;
};