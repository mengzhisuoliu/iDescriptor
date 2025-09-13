#include "devdiskmanager.h"
#include "iDescriptor.h"
#include "settingsmanager.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

DevDiskManager *DevDiskManager::sharedInstance()
{
    static DevDiskManager instance;
    return &instance;
}

DevDiskManager::DevDiskManager(QObject *parent) : QObject{parent}
{
    m_networkManager = new QNetworkAccessManager(this);
    m_isImageListReady = false; // Explicitly set initial state
    fetchImageList();
}

QNetworkReply *DevDiskManager::fetchImageList()
{
    QUrl url("https://raw.githubusercontent.com/uncor3/resources/refs/heads/"
             "main/DeveloperDiskImages.json");
    QNetworkRequest request(url);
    auto *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit imageListFetched(false, reply->errorString());
        } else {
            m_imageListJsonData = reply->readAll();
            m_isImageListReady = true; // Set the flag on success
            emit imageListFetched(true);
        }
        reply->deleteLater();
    });

    return reply;
}

QMap<QString, QMap<QString, QString>> DevDiskManager::parseDiskDir()
{
    QJsonDocument doc = QJsonDocument::fromJson(m_imageListJsonData);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON response from image list API";
        return {};
    }

    QMap<QString, QMap<QString, QString>>
        imageFiles; // version -> {type -> url}

    QJsonObject root = doc.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QString version = it.key();
        const QJsonObject versionData = it.value().toObject();

        // Skip special entries
        if (version == "Fallback") {
            continue;
        }

        QMap<QString, QString> versionFiles;

        // Handle Image URLs
        if (versionData.contains("Image")) {
            QJsonArray imageArray = versionData["Image"].toArray();
            if (!imageArray.isEmpty()) {
                versionFiles["DeveloperDiskImage.dmg"] =
                    imageArray[0].toString();
            }
        }

        // Handle Signature URLs
        if (versionData.contains("Signature")) {
            QJsonArray sigArray = versionData["Signature"].toArray();
            if (!sigArray.isEmpty()) {
                versionFiles["DeveloperDiskImage.dmg.signature"] =
                    sigArray[0].toString();
            }
        }

        // Handle Trustcache URLs (for iOS 17+)
        if (versionData.contains("Trustcache")) {
            QJsonArray trustcacheArray = versionData["Trustcache"].toArray();
            if (!trustcacheArray.isEmpty()) {
                versionFiles["Image.dmg.trustcache"] =
                    trustcacheArray[0].toString();
            }
        }

        // Handle BuildManifest URLs (for iOS 17+)
        if (versionData.contains("BuildManifest")) {
            QJsonArray manifestArray = versionData["BuildManifest"].toArray();
            if (!manifestArray.isEmpty()) {
                versionFiles["BuildManifest.plist"] =
                    manifestArray[0].toString();
            }
        }

        // Only add versions that have at least an image file
        if (!versionFiles.isEmpty() &&
            versionFiles.contains("DeveloperDiskImage.dmg")) {
            imageFiles[version] = versionFiles;
        }
    }

    return imageFiles;
}

GetImagesSortedFinalResult
DevDiskManager::parseImageList(int deviceMajorVersion, int deviceMinorVersion,
                               const char *mounted_sig,
                               uint64_t mounted_sig_len)
{
    m_availableImages.clear();
    QStringList compatibleVersions = QStringList();
    QStringList otherVersions = QStringList();

    QMap<QString, QMap<QString, QString>> imageFiles = parseDiskDir();
    GetImagesSortedResult sortedResult =
        getImagesSorted(imageFiles, deviceMajorVersion, deviceMinorVersion,
                        mounted_sig, mounted_sig_len);
    sortVersions(sortedResult);

    QList<ImageInfo> compatibleResult;
    for (const QString &version : sortedResult.compatibleImages) {
        compatibleResult.append(m_availableImages[version]);
    }
    QList<ImageInfo> otherResult;
    for (const QString &version : sortedResult.otherImages) {
        otherResult.append(m_availableImages[version]);
    }

    return GetImagesSortedFinalResult{compatibleResult, otherResult};
}

void DevDiskManager::sortVersions(GetImagesSortedResult &sortedResult)
{
    QStringList &compatibleVersions = sortedResult.compatibleImages;
    QStringList &otherVersions = sortedResult.otherImages;
    auto versionSort = [](const QString &a, const QString &b) {
        QStringList aParts = a.split('.');
        QStringList bParts = b.split('.');

        for (int i = 0; i < qMax(aParts.size(), bParts.size()); ++i) {
            int aNum = (i < aParts.size()) ? aParts[i].toInt() : 0;
            int bNum = (i < bParts.size()) ? bParts[i].toInt() : 0;

            if (aNum != bNum) {
                return aNum > bNum; // Descending order (newest first)
            }
        }
        return false;
    };

    std::sort(compatibleVersions.begin(), compatibleVersions.end(),
              versionSort);
    std::sort(otherVersions.begin(), otherVersions.end(), versionSort);
}

GetImagesSortedResult DevDiskManager::getImagesSorted(
    QMap<QString, QMap<QString, QString>> imageFiles, int deviceMajorVersion,
    int deviceMinorVersion, const char *mounted_sig, uint64_t mounted_sig_len)
{

    QStringList compatibleVersions = QStringList();
    QStringList otherVersions = QStringList();
    // TODO: what is this ?
    bool hasConnectedDevice = (deviceMajorVersion > 0);

    for (auto it = imageFiles.constBegin(); it != imageFiles.constEnd(); ++it) {
        if (it.value().contains("DeveloperDiskImage.dmg") &&
            it.value().contains("DeveloperDiskImage.dmg.signature")) {
            QString version = it.key();

            ImageInfo info;
            info.version = version;
            info.dmgPath = it.value()["DeveloperDiskImage.dmg"];
            info.sigPath = it.value()["DeveloperDiskImage.dmg.signature"];
            info.isDownloaded = isImageDownloaded(
                version, SettingsManager::sharedInstance()->devdiskimgpath());

            // Determine compatibility
            if (hasConnectedDevice) {
                QStringList versionParts = version.split('.');
                if (versionParts.size() >= 1) {
                    bool ma_ok;
                    bool mi_ok;
                    int imageMajorVersion = versionParts[0].toInt(&ma_ok);
                    int imageMinorVersion = (versionParts.size() >= 2)
                                                ? versionParts[1].toInt(&mi_ok)
                                                : 0;
                    if (ma_ok && mi_ok) {
                        if (deviceMajorVersion >= 16) {
                            info.isCompatible = (imageMajorVersion == 16);
                        } else {
                            // FIXME: this seems to work only for older iphones
                            // so commented out but in the future , it may be
                            // enabled (imageMajorVersion ==
                            // deviceMajorVersion);
                            if (imageMajorVersion == deviceMajorVersion &&
                                imageMinorVersion == deviceMinorVersion) {
                                info.isCompatible = true;
                            }
                        }
                    }
                }
            }

            // Check if mounted
            if (info.isCompatible && info.isDownloaded && mounted_sig) {
                QString sigLocalPath =
                    QDir(
                        QDir(
                            SettingsManager::sharedInstance()->devdiskimgpath())
                            .filePath(version))
                        .filePath("DeveloperDiskImage.dmg.signature");
                info.isMounted =
                    compareSignatures(sigLocalPath.toUtf8().constData(),
                                      mounted_sig, mounted_sig_len);
            }

            m_availableImages[version] = info;

            if (info.isCompatible) {
                compatibleVersions.append(version);
            } else {
                otherVersions.append(version);
            }
        }
    }
    return GetImagesSortedResult{compatibleVersions, otherVersions};
}

QList<ImageInfo> DevDiskManager::getAllImages() const
{
    return m_availableImages.values();
}

QPair<QNetworkReply *, QNetworkReply *>
DevDiskManager::downloadImage(const QString &version)
{
    qDebug() << "Request to download image version:" << version;
    if (!m_availableImages.contains(version)) {
        qDebug() << "Image not found:" << version;
        emit imageDownloadFinished(version, false, "Image version not found.");
        return {nullptr, nullptr};
    }

    QString targetDir =
        QDir(SettingsManager::sharedInstance()->devdiskimgpath())
            .filePath(version);
    if (!QDir().mkpath(targetDir)) {
        qDebug() << "Could not create directory:" << targetDir;
        emit imageDownloadFinished(
            version, false,
            QString("Could not create directory: %1").arg(targetDir));
        return {nullptr, nullptr};
    }

    const ImageInfo &info = m_availableImages[version];

    QUrl dmgUrl(info.dmgPath);
    QNetworkRequest dmgRequest(dmgUrl);
    QNetworkReply *dmgReply = m_networkManager->get(dmgRequest);

    QUrl sigUrl(info.sigPath);
    QNetworkRequest sigRequest(sigUrl);
    QNetworkReply *sigReply = m_networkManager->get(sigRequest);

    return {dmgReply, sigReply};
}

bool DevDiskManager::isImageDownloaded(const QString &version,
                                       const QString &downloadPath) const
{
    QString versionPath = QDir(downloadPath).filePath(version);
    QString dmgPath = QDir(versionPath).filePath("DeveloperDiskImage.dmg");
    QString sigPath =
        QDir(versionPath).filePath("DeveloperDiskImage.dmg.signature");

    return QFile::exists(dmgPath) && QFile::exists(sigPath);
}

bool DevDiskManager::downloadCompatibleImageInternal(iDescriptorDevice *device)
{

    unsigned int device_version = idevice_get_device_version(device->device);
    unsigned int deviceMajorVersion = (device_version >> 16) & 0xFF;
    unsigned int deviceMinorVersion = (device_version >> 8) & 0xFF;
    qDebug() << "Device version:" << deviceMajorVersion << "."
             << deviceMinorVersion;
    GetImagesSortedFinalResult images =
        parseImageList(deviceMajorVersion, deviceMinorVersion, "", 0);

    for (const ImageInfo &info : images.compatibleImages) {
        if (info.isDownloaded) {
            qDebug() << "There is a compatible image already downloaded:"
                     << info.version;
            return true;
        }
    }

    // If none are downloaded, download the newest compatible one
    if (!images.compatibleImages.isEmpty()) {
        const QString versionToDownload =
            images.compatibleImages.first().version;
        qDebug() << "No compatible image found locally. Downloading version:"
                 << versionToDownload;

        QPair<QNetworkReply *, QNetworkReply *> replies =
            downloadImage(versionToDownload);
        auto *downloadItem = new DownloadItem();
        downloadItem->version = versionToDownload;
        downloadItem->downloadPath =
            SettingsManager::sharedInstance()->devdiskimgpath();
        downloadItem->dmgReply = replies.first;
        downloadItem->sigReply = replies.second;

        connect(downloadItem->dmgReply, &QNetworkReply::downloadProgress, this,
                &DevDiskManager::onDownloadProgress);
        connect(downloadItem->dmgReply, &QNetworkReply::finished, this,
                &DevDiskManager::onFileDownloadFinished);
        connect(downloadItem->sigReply, &QNetworkReply::downloadProgress, this,
                &DevDiskManager::onDownloadProgress);
        connect(downloadItem->sigReply, &QNetworkReply::finished, this,
                &DevDiskManager::onFileDownloadFinished);

        m_activeDownloads[downloadItem->dmgReply] = downloadItem;
        m_activeDownloads[downloadItem->sigReply] = downloadItem;
        return true; // Indicate that the async operation has started
    }

    qDebug() << "No compatible image found to mount on device:"
             << device->udid.c_str();

    return false;
}

bool DevDiskManager::downloadCompatibleImage(iDescriptorDevice *device)
{
    if (m_isImageListReady) {
        // If the list is already fetched, run the logic immediately.
        return downloadCompatibleImageInternal(device);
    } else {
        // Otherwise, connect to the signal and wait.
        qDebug() << "Image list not ready, waiting for it to be fetched...";
        connect(
            this, &DevDiskManager::imageListFetched, this,
            [this, device](bool success) {
                if (success) {
                    qDebug() << "Image list is now ready. Retrying download...";
                    downloadCompatibleImageInternal(device);
                } else {
                    qDebug() << "Failed to fetch image list. Cannot download.";
                }
            },
            Qt::SingleShotConnection);

        // The operation is now asynchronous, the immediate return value
        // indicates that the process has started.
        return true;
    }
}

// TODO: boolean to download
bool DevDiskManager::mountCompatibleImageInternal(iDescriptorDevice *device)
{
    GetMountedImageResult res = getMountedImage(device->udid.c_str());
    if (res.success) {
        qDebug() << "An image is already mounted on device:"
                 << device->udid.c_str();
        return true;
    }

    unsigned int device_version = idevice_get_device_version(device->device);
    unsigned int deviceMajorVersion = (device_version >> 16) & 0xFF;
    unsigned int deviceMinorVersion = (device_version >> 8) & 0xFF;

    // TODO: use actual device version
    GetImagesSortedFinalResult images =
        parseImageList(deviceMajorVersion, deviceMinorVersion,
                       res.output.c_str(), res.output.length());

    // 1. Try to mount an already downloaded compatible image
    for (const ImageInfo &info : images.compatibleImages) {
        if (info.isDownloaded) {
            qDebug() << "There is a compatible image already downloaded:"
                     << info.version;
            qDebug() << "Attempting to mount image version" << info.version
                     << "on device:" << device->udid.c_str();
            if (mountImage(info.version, device->udid.c_str())) {
                qDebug() << "Mounted existing image version" << info.version
                         << "on device:" << device->udid.c_str();
                return true;
            } else {
                qDebug() << "Failed to mount existing image version"
                         << info.version
                         << "on device:" << device->udid.c_str();
                return false;
            }
        }
    }
    const QString downloadPath =
        SettingsManager::sharedInstance()->devdiskimgpath();

    // 2. If none are downloaded, download the newest compatible one
    if (!images.compatibleImages.isEmpty()) {
        const QString versionToDownload =
            images.compatibleImages.first().version;
        qDebug() << "No compatible image found locally. Downloading version:"
                 << versionToDownload;

        // Connect a one-time slot to mount the image after download finishes
        connect(
            this, &DevDiskManager::imageDownloadFinished, this,
            [this, device, downloadPath,
             versionToDownload](const QString &finishedVersion, bool success,
                                const QString &errorMessage) {
                if (success && finishedVersion == versionToDownload) {
                    qDebug() << "Download finished for" << finishedVersion
                             << ". Now attempting to mount.";
                    mountImage(finishedVersion, device->udid.c_str());
                    // TODO: You might want to emit another signal here to
                    // notify the UI of the final mount result.
                } else if (!success) {
                    qDebug() << "Failed to download" << finishedVersion << ":"
                             << errorMessage;
                }
            },
            Qt::SingleShotConnection);

        // Start the download
        QPair<QNetworkReply *, QNetworkReply *> replies =
            downloadImage(versionToDownload);
        auto *downloadItem = new DownloadItem();
        downloadItem->version = versionToDownload;
        downloadItem->downloadPath = downloadPath;
        downloadItem->dmgReply = replies.first;
        downloadItem->sigReply = replies.second;

        connect(downloadItem->dmgReply, &QNetworkReply::downloadProgress, this,
                &DevDiskManager::onDownloadProgress);
        connect(downloadItem->dmgReply, &QNetworkReply::finished, this,
                &DevDiskManager::onFileDownloadFinished);
        connect(downloadItem->sigReply, &QNetworkReply::downloadProgress, this,
                &DevDiskManager::onDownloadProgress);
        connect(downloadItem->sigReply, &QNetworkReply::finished, this,
                &DevDiskManager::onFileDownloadFinished);

        m_activeDownloads[downloadItem->dmgReply] = downloadItem;
        m_activeDownloads[downloadItem->sigReply] = downloadItem;
        return true; // Indicate that the async operation has started
    }

    qDebug() << "No compatible image found to mount on device:"
             << device->udid.c_str();

    return false;
}

bool DevDiskManager::mountCompatibleImage(iDescriptorDevice *device)
{
    if (m_isImageListReady) {
        // If the list is already fetched, run the logic immediately.
        return mountCompatibleImageInternal(device);
    } else {
        // Otherwise, connect to the signal and wait.
        qDebug() << "Image list not ready, waiting for it to be fetched...";
        connect(
            this, &DevDiskManager::imageListFetched, this,
            [this, device](bool success) {
                if (success) {
                    qDebug() << "Image list is now ready. Retrying mount...";
                    mountCompatibleImageInternal(device);
                } else {
                    qDebug() << "Failed to fetch image list. Cannot mount.";
                }
            },
            Qt::SingleShotConnection);

        // The operation is now asynchronous, the immediate return value
        // indicates that the process has started.
        return true;
    }
}

bool DevDiskManager::mountImage(const QString &version, const QString &udid)
{
    const QString downloadPath =
        SettingsManager::sharedInstance()->devdiskimgpath();
    if (!isImageDownloaded(version, downloadPath)) {
        return false;
    }

    QString versionPath = QDir(downloadPath).filePath(version);
    return mount_dev_image(udid.toUtf8().constData(),
                           versionPath.toUtf8().constData());
}

void DevDiskManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || !m_activeDownloads.contains(reply))
        return;

    auto *item = m_activeDownloads[reply];

    if (reply->property("totalSizeAdded").isNull() && bytesTotal > 0) {
        item->totalSize += bytesTotal;
        reply->setProperty("totalSizeAdded", true);
    }

    if (reply == item->dmgReply) {
        item->dmgReceived = bytesReceived;
    } else if (reply == item->sigReply) {
        item->sigReceived = bytesReceived;
    }

    qint64 totalReceived = item->dmgReceived + item->sigReceived;

    if (item->totalSize > 0) {
        emit imageDownloadProgress(item->version,
                                   (totalReceived * 100) / item->totalSize);
    }
}

void DevDiskManager::onFileDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || !m_activeDownloads.contains(reply))
        return;

    auto *item = m_activeDownloads[reply];
    m_activeDownloads.remove(reply);

    if (reply->error() != QNetworkReply::NoError) {
        emit imageDownloadFinished(item->version, false, reply->errorString());

        if (reply == item->dmgReply && item->sigReply)
            item->sigReply->abort();
        if (reply == item->sigReply && item->dmgReply)
            item->dmgReply->abort();

        if (m_activeDownloads.key(item) == nullptr) {
            delete item;
        }
        reply->deleteLater();
        return;
    }

    QString path = QUrl::fromPercentEncoding(reply->url().path().toUtf8());
    QFileInfo fileInfo(path);
    QString filename = fileInfo.fileName();
    // TODO
    // QString targetPath =
    // QDir(QDir(item->downloadPath).filePath(item->version))

    // TODO: change to settings path
    QString targetPath = QDir(QDir("./devdiskimages").filePath(item->version))
                             .filePath(filename);

    QFile file(targetPath);
    qDebug() << "Saving downloaded file to:" << targetPath;
    if (!file.open(QIODevice::WriteOnly)) {
        emit imageDownloadFinished(
            item->version, false,
            QString("Could not save file: %1").arg(targetPath));
    } else {
        file.write(reply->readAll());
        file.close();
    }

    reply->deleteLater();

    if (m_activeDownloads.key(item) == nullptr) { // Both files finished
        emit imageDownloadFinished(item->version, true);
        delete item;
    }
}

bool DevDiskManager::unmountImage()
{
    // Implementation for unmounting the currently mounted disk image
    return false; // TODO: Implement when unmount functionality is available
}

bool DevDiskManager::compareSignatures(const char *signature_file_path,
                                       const char *mounted_sig,
                                       uint64_t mounted_sig_len)
{
    FILE *f_sig = fopen(signature_file_path, "rb");
    if (!f_sig) {
        qDebug() << "ERROR: Could not open signature file:"
                 << signature_file_path;
        return false;
    }

    fseek(f_sig, 0, SEEK_END);
    long local_sig_len = ftell(f_sig);
    fseek(f_sig, 0, SEEK_SET);

    char *local_sig = (char *)malloc(local_sig_len);
    if (!local_sig) {
        fclose(f_sig);
        return false;
    }

    fread(local_sig, 1, local_sig_len, f_sig);
    fclose(f_sig);

    bool matches = false;
    if ((mounted_sig_len == (uint64_t)local_sig_len) &&
        (memcmp(mounted_sig, local_sig, mounted_sig_len) == 0)) {
        qDebug() << "Signatures match!";
        matches = true;
    } else {
        qDebug() << "Signatures DO NOT match!";
    }

    free(local_sig);
    return matches;
}

GetMountedImageResult DevDiskManager::getMountedImage(const char *udid)
{
    QPair<bool, plist_t> result = _get_mounted_image(udid);

    if (result.first == false) {
        plist_t sig_err = plist_dict_get_item(result.second, "Error");
        // TODO: should print ?
        plist_print(result.second);
        if (sig_err) {
            char *error = NULL;
            plist_get_string_val(sig_err, &error);
            if (error == "DeviceLocked") {
                qDebug() << "Error:" << error;
                free(error);
                plist_free(result.second);
                return GetMountedImageResult{false, "", "Device is locked"};
            }
        } else {
            return GetMountedImageResult{false, "", "Unknown error"};
        }
    }

    plist_t sig_array_node =
        plist_dict_get_item(result.second, "ImageSignature");
    if (sig_array_node == NULL) {
        plist_free(result.second);
        return GetMountedImageResult{false, "", "No disk image mounted"};
    }

    char *mounted_sig = nullptr;
    uint64_t mounted_sig_len = 0;

    if (sig_array_node && plist_get_node_type(sig_array_node) == PLIST_ARRAY &&
        plist_array_get_size(sig_array_node) > 0) {
        plist_t sig_data_node = plist_array_get_item(sig_array_node, 0);
        if (sig_data_node && plist_get_node_type(sig_data_node) == PLIST_DATA) {
            plist_get_data_val(sig_data_node, &mounted_sig, &mounted_sig_len);
        }
    }
    std::string mounted_sig_str(mounted_sig ? mounted_sig : "");
    free(mounted_sig);
    plist_free(result.second);
    if (mounted_sig_str.empty()) {
        return GetMountedImageResult{
            false, "", "No disk image mounted (No signature found)"};
    }
    return GetMountedImageResult{true, mounted_sig_str, "Success"};
}

bool DevDiskManager::isImageListReady() const { return m_isImageListReady; }