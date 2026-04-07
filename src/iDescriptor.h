/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
/* required for mingw64 */
#ifdef WIN32
#include <cstdint>
using u_int8_t = uint8_t;
using u_int16_t = uint16_t;
using u_int32_t = uint32_t;
using u_int64_t = uint64_t;
#endif
#include <QDebug>
#include <QImage>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QRegularExpression>
#include <QThread>
#include <QtCore/QObject>

#include "service.h"
#include <mutex>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#define TOOL_NAME "iDescriptor"
#define APP_LABEL "iDescriptor"
#define APP_COPYRIGHT                                                          \
    "© 2026 The iDescriptor Project contributors. See AUTHORS for details."
#define RECOVERY_CLIENT_CONNECTION_TRIES 3
#define APPLE_VENDOR_ID 0x05ac
#define REPO_URL "https://github.com/iDescriptor/iDescriptor"
#define SPONSORS_JSON_URL                                                      \
    "https://raw.githubusercontent.com/iDescriptor/iDescriptor/refs/heads/"    \
    "main/sponsors.json"
#define DEVELOPER_DISK_IMAGE_JSON_URL                                          \
    "https://raw.githubusercontent.com/iDescriptor/iDescriptor/refs/heads/"    \
    "main/DeveloperDiskImages.json"

#include "iDescriptor-utils.h"
#ifdef ENABLE_RECOVERY_DEVICE_SUPPORT
#include <libirecovery.h>
#endif

#define DeviceLockedMountErrorCode -21
#define NotFoundErrorCode -14
#define ServiceNotFoundErrorCode 21
#define PairingDialogResponsePending -28
#define InvalidHostID -10
#define PasswordProtected -30
#define InvalidServiceErrorCode -59
#define TimeoutErrorCode -71

#define DISK_IMAGE_TYPE_DEVELOPER "Developer"
#define PHOTOS_SQLITE_DB_PATH "/PhotoData/Photos.sqlite"

#define HEARTBEAT_RETRY_LIMIT 2

#define DONATE_URL "https://opencollective.com/idescriptor"

#ifdef __linux__
#define LOCKDOWN_PATH "/var/lib/lockdown"
#elif __APPLE__
#define LOCKDOWN_PATH "/var/db/lockdown"
#else
/* Windows */
#define LOCKDOWN_PATH qgetenv("PROGRAMDATA") + "/Apple/Lockdown"
#endif

// rust codebase
#include "idescriptor_rust_codebase/src/afc2_services.cxxqt.h"
#include "idescriptor_rust_codebase/src/afc_services.cxxqt.h"
#include "idescriptor_rust_codebase/src/hause_arrest.cxxqt.h"
#include "idescriptor_rust_codebase/src/io_manager.cxxqt.h"
#include "idescriptor_rust_codebase/src/lib.cxxqt.h"
#include "idescriptor_rust_codebase/src/screenshot.cxxqt.h"
#include "idescriptor_rust_codebase/src/service_manager.cxxqt.h"

namespace iDescriptor
{
enum IdeviceConnectionType { CONNECTION_USB = 1, CONNECTION_NETWORK = 2 };
}

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

struct DeviceVersion {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};

// Carefull not all the vars are initialized in init_device.cpp
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
    std::string serialNumber;
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
    DeviceVersion parsedDeviceVersion;
    std::string wifiMacAddress;
    bool isWireless = false;
    // empty on USB devices
    std::string ipAddress;
    /* same as udid on iDescriptorDevice */
    std::string UniqueDeviceID;
};

struct iDescriptorDevice {
    QString udid;
    iDescriptor::IdeviceConnectionType conn_type;
    DeviceInfo deviceInfo;
    unsigned int ios_version;
    CXX::ServiceManager *service_manager;
    CXX::AfcBackend *afc_backend;
    CXX::Afc2Backend *afc2_backend;
};

void fullDeviceInfo(const pugi::xml_document &doc, DeviceInfo &d);

#ifdef ENABLE_RECOVERY_DEVICE_SUPPORT
struct iDescriptorRecoveryDevice {
    uint64_t ecid;
    irecv_mode mode;
    uint32_t cpid;
    uint32_t bdid;
    std::string displayName;
    std::recursive_mutex mutex;
};
struct iDescriptorInitDeviceResultRecovery {
    irecv_client_t client = nullptr;
    irecv_device_info deviceInfo;
    irecv_error_t error;
    bool success = false;
    irecv_mode mode = IRECV_K_RECOVERY_MODE_1;
    const char *displayName = nullptr;
};

std::string parse_recovery_mode(irecv_mode productType);

void init_idescriptor_recovery_device(uint64_t ecid,
                                      iDescriptorInitDeviceResultRecovery &res);
#endif

enum class AddType {
    Regular,
    Pairing,
    FailedToPair,
    Wireless,
    UpgradeToWireless
};

std::string parse_product_type(const std::string &productType);

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

void fetchAppIconFromApple(
    QNetworkAccessManager *manager, const QString &bundleId,
    std::function<void(const QPixmap &, const QJsonObject &)> callback);

struct NetworkDevice {
    QString name;                           // service name
    QString hostname;                       // e.g., iPhone-2.local
    QString address;                        // IPv4 or IPv6 address
    uint16_t port = 22;                     // SSH port
    std::map<std::string, std::string> txt; // TXT records
    QString macAddress;                     // MAC address if available
    bool operator==(const NetworkDevice &other) const
    {
        return name == other.name && address == other.address;
    }
};

QPixmap load_heic(const QByteArray &data);

// Helper struct for semantic version comparison
struct AppVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;

    static AppVersion fromString(QString versionString)
    {
        // Keep only digits and dots for comparison
        versionString.remove(QRegularExpression("[^\\d.]"));
        AppVersion v;
        QStringList parts = versionString.split('.');
        if (parts.size() > 0)
            v.major = parts[0].toInt();
        if (parts.size() > 1)
            v.minor = parts[1].toInt();
        if (parts.size() > 2)
            v.patch = parts[2].toInt();
        return v;
    }

    bool operator<(const AppVersion &other) const
    {
        if (major != other.major)
            return major < other.major;
        if (minor != other.minor)
            return minor < other.minor;
        return patch < other.patch;
    }

    bool operator==(const AppVersion &other) const
    {
        return major == other.major && minor == other.minor &&
               patch == other.patch;
    }

    bool operator>(const AppVersion &other) const
    {
        return !(*this < other || *this == other);
    }
    bool operator<=(const AppVersion &other) const
    {
        return (*this < other || *this == other);
    }
    bool operator>=(const AppVersion &other) const { return !(*this < other); }
};

// Checks if the current app version matches a given version condition
inline bool versionMatches(const QString &currentVersionStr,
                           const QString &conditionStr)
{
    AppVersion currentVersion = AppVersion::fromString(currentVersionStr);
    AppVersion conditionVersion = AppVersion::fromString(conditionStr);

    if (conditionStr.startsWith("<="))
        return currentVersion <= conditionVersion;
    if (conditionStr.startsWith(">="))
        return currentVersion >= conditionVersion;
    if (conditionStr.startsWith("<"))
        return currentVersion < conditionVersion;
    if (conditionStr.startsWith(">"))
        return currentVersion > conditionVersion;

    // Exact match
    return currentVersion == conditionVersion;
}

inline QJsonObject getVersionedConfig(const QJsonObject &rootObj)
{
    QStringList keys = rootObj.keys();
    for (const QString &key : keys) {
        if (versionMatches(APP_VERSION, key)) {
            qDebug() << "getVersionedConfig picked version:" << key;
            return rootObj[key].toObject();
        }
    }
    return QJsonObject();
}

struct XmlPlistDict {
    pugi::xml_node current_node;

    XmlPlistDict() = default;
    explicit XmlPlistDict(pugi::xml_node n) : current_node(n) {}

    bool valid() const { return current_node; }

    bool isDict() const
    {
        return current_node && std::strcmp(current_node.name(), "dict") == 0;
    }

    bool isArray() const
    {
        return current_node && std::strcmp(current_node.name(), "array") == 0;
    }

private:
    // helper: for dict lookups
    pugi::xml_node findValueNode(const char *key) const
    {
        if (!isDict())
            return {};

        for (pugi::xml_node child = current_node.first_child(); child;
             child = child.next_sibling()) {
            if (std::strcmp(child.name(), "key") == 0 &&
                std::strcmp(child.text().as_string(), key) == 0) {
                return child.next_sibling(); // the value node
            }
        }
        return {};
    }

public:
    // dict key access
    XmlPlistDict operator[](const char *key) const
    {
        return XmlPlistDict(findValueNode(key));
    }

    XmlPlistDict operator[](const std::string &key) const
    {
        return (*this)[key.c_str()];
    }

    XmlPlistDict operator[](const QString &key) const
    {
        return (*this)[key.toUtf8().constData()];
    }

    // array index access
    XmlPlistDict operator[](int index) const
    {
        if (!isArray() || index < 0)
            return XmlPlistDict();

        int i = 0;
        for (pugi::xml_node child = current_node.first_child(); child;
             child = child.next_sibling()) {
            if (!child.name() || !*child.name())
                continue; // skip text/whitespace
            if (i == index)
                return XmlPlistDict(child);
            ++i;
        }
        return XmlPlistDict();
    }

    // getters on current node (like PlistNavigator)
    bool getBool(bool def = false) const
    {
        if (!current_node)
            return def;

        const char *name = current_node.name();
        if (!std::strcmp(name, "true"))
            return true;
        if (!std::strcmp(name, "false"))
            return false;

        std::string s = current_node.text().as_string();
        if (s == "true" || s == "1")
            return true;
        if (s == "false" || s == "0")
            return false;
        return def;
    }

    uint64_t getUInt(uint64_t def = 0) const
    {
        if (!current_node)
            return def;
        std::string s = current_node.text().as_string();
        if (s.empty())
            return def;
        try {
            return std::stoull(s);
        } catch (...) {
            return def;
        }
    }

    std::string getString(const std::string &def = std::string()) const
    {
        if (!current_node)
            return def;
        return current_node.text().as_string();
    }

    pugi::xml_node getNode() const { return current_node; }
};

void parseOldDeviceBattery(XmlPlistDict &ioreg, DeviceInfo &d);
void parseDeviceBattery(XmlPlistDict &ioreg, DeviceInfo &d);