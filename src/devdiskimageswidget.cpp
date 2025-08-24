#include "devdiskimageswidget.h"
#include "appcontext.h"
#include "iDescriptor.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStringList>
#include <QVBoxLayout>

// Handle errors , event though it failed, ui thinks it is mounted
/*DetailedError: Error Domain=com.apple.MobileStorage.ErrorDomain Code=-2
 * "Failed to mount
 * /private/var/run/mobile_image_mounter/42B093B66120045164A6781BD419867320D74D62DA078BB73979CD87C9AC14ECF4331C385F72EAF1F68C81C922D2EF3DE647BC0949DEE6557FBC06DAF7C13FEC/E9E8F8B5021B74DF4C10E6595BB4C16BB45CC55503B8B32085CE31C20A62812D4DB8264F758D5256DBA697139E56F2E8BEE3690EE33F252D17C044BB6C0446A2/tonVFp.dmg."
 * UserInfo={NSLocalizedDescription=Failed to mount
 * /private/var/run/mobile_image_mounter/42B093B66120045164A6781BD419867320D74D62DA078BB73979CD87C9AC14ECF4331C385F72EAF1F68C81C922D2EF3DE647BC0949DEE6557FBC06DAF7C13FEC/E9E8F8B5021B74DF4C10E6595BB4C16BB45CC55503B8B32085CE31C20A62812D4DB8264F758D5256DBA697139E56F2E8BEE3690EE33F252D17C044BB6C0446A2/tonVFp.dmg.,
 * NSUnderlyingError=0x12fe05ce0 {Error
 * Domain=com.apple.MobileStorage.ErrorDomain Code=-2 "Invalid value for
 * MountPath: Error Domain=com.apple.MobileStorage.ErrorDomain Code=-3 "A disk
 * image of type Developer/(null) is already mounted at /Developer."
 * UserInfo={NSLocalizedDescription=A disk image of type Developer/(null) is
 * already mounted at /Developer.}" UserInfo={NSLocalizedDescription=Invalid
 * value for MountPath: Error Domain=com.apple.MobileStorage.ErrorDomain Code=-3
 * "A disk image of type Developer/(null) is already mounted at /Developer."
 * UserInfo={NSLocalizedDescription=A disk image of type Developer/(null) is
 * already mounted at /Developer.}}}}*/

extern bool mount_dev_image(const char *udid, const char *image_dir_path);

DevDiskImagesWidget::DevDiskImagesWidget(iDescriptorDevice *device,
                                         QWidget *parent)
    : QWidget{parent}, m_networkManager(new QNetworkAccessManager(this)),
      m_currentDevice(device)
{
    setupUi();
    fetchImageList();
    updateDeviceList();
    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            &DevDiskImagesWidget::updateDeviceList);
    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            &DevDiskImagesWidget::updateDeviceList);
    connect(m_deviceComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DevDiskImagesWidget::onDeviceSelectionChanged);
}

void DevDiskImagesWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("Download Path:")));
    m_downloadPathEdit = new QLineEdit();
    m_downloadPathEdit->setReadOnly(true);
    pathLayout->addWidget(m_downloadPathEdit);
    auto *changeDirButton = new QPushButton(tr("Change..."));
    connect(changeDirButton, &QPushButton::clicked, this,
            &DevDiskImagesWidget::changeDownloadDirectory);
    pathLayout->addWidget(changeDirButton);
    layout->addLayout(pathLayout);

    auto *mountLayout = new QHBoxLayout();
    mountLayout->addWidget(new QLabel(tr("Device:")));
    m_deviceComboBox = new QComboBox(this);
    mountLayout->addWidget(m_deviceComboBox);
    m_mountButton = new QPushButton(tr("Mount"), this);
    connect(m_mountButton, &QPushButton::clicked, this,
            &DevDiskImagesWidget::onMountButtonClicked);
    mountLayout->addWidget(m_mountButton);
    layout->addLayout(mountLayout);

    m_stackedWidget = new QStackedWidget(this);
    layout->addWidget(m_stackedWidget);

    m_initialStatusLabel = new QLabel("Fetching image list...");
    m_initialStatusLabel->setAlignment(Qt::AlignCenter);
    m_stackedWidget->addWidget(m_initialStatusLabel);

    m_errorWidget = new QWidget(this);
    QPushButton *retryButton = new QPushButton(tr("Retry"), m_errorWidget);
    connect(retryButton, &QPushButton::clicked, this,
            &DevDiskImagesWidget::fetchImageList);

    auto *errorLayout = new QVBoxLayout(m_errorWidget);
    m_statusLabel = new QLabel("");
    errorLayout->addWidget(m_statusLabel);
    errorLayout->addWidget(retryButton);
    errorLayout->addStretch();
    m_stackedWidget->addWidget(m_errorWidget);

    m_imageListWidget = new QListWidget(this);
    m_stackedWidget->addWidget(m_imageListWidget);

    m_downloadPath =
        QDir(QCoreApplication::applicationDirPath()).filePath("devdiskimages");
    m_downloadPathEdit->setText(m_downloadPath);
}

void DevDiskImagesWidget::fetchImageList()
{
    m_stackedWidget->setCurrentWidget(m_initialStatusLabel);

    QUrl url("https://api.github.com/repos/mspvirajpatel/"
             "Xcode_Developer_Disk_Images/git/trees/master?recursive=true");
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onImageListFetchFinished(reply); });
}

void DevDiskImagesWidget::onImageListFetchFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        m_errorWidget->setVisible(true);
        m_statusLabel->setText(reply->errorString());
        m_stackedWidget->setCurrentWidget(m_errorWidget);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    m_imageListJsonData = data;
    parseAndDisplayImages(m_imageListJsonData);
    m_stackedWidget->setCurrentWidget(m_imageListWidget);
}

void DevDiskImagesWidget::onDeviceSelectionChanged(int index)
{
    if (index < 0 ||
        index >= AppContext::sharedInstance()->getAllDevices().size()) {
        m_currentDevice = nullptr;
    } else {
        m_currentDevice = AppContext::sharedInstance()->getAllDevices()[index];
    }
    parseAndDisplayImages(m_imageListJsonData);
}

void DevDiskImagesWidget::parseAndDisplayImages(const QByteArray &jsonData)
{
    m_imageListWidget->clear();
    m_availableImages.clear();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        m_statusLabel->setText(tr("Invalid image list format."));
        m_stackedWidget->setCurrentWidget(m_errorWidget);
        return;
    }

    QMap<QString, QMap<QString, QString>>
        imageFiles; // dir -> {filename -> path}

    QJsonArray tree = doc.object()["tree"].toArray();
    for (const QJsonValue &value : tree) {
        QJsonObject obj = value.toObject();
        QString path = obj["path"].toString();
        if (path.endsWith(".dmg") || path.endsWith(".dmg.signature")) {
            QFileInfo fileInfo(path);
            QString dir = fileInfo.path();
            QString filename = fileInfo.fileName();
            if (!dir.isEmpty() && dir != ".")
                imageFiles[dir][filename] = path;
        }
    }

    // Get device iOS version for compatibility checking
    QString deviceVersion;
    int deviceMajorVersion = 0;
    int deviceMinorVersion = 0;
    bool hasConnectedDevice = false;

    if (m_currentDevice && m_currentDevice->device) {
        // TODO : use the macro IDEVICE_DEVICE_VERSION
        unsigned int device_version =
            idevice_get_device_version(m_currentDevice->device);
        deviceMajorVersion = (device_version >> 16) & 0xFF;
        deviceMinorVersion = (device_version >> 8) & 0xFF;
        deviceVersion =
            QString("%1.%2").arg(deviceMajorVersion).arg(deviceMinorVersion);
        hasConnectedDevice = true;
    }

    qDebug() << "Has connected device:" << hasConnectedDevice;

    // Separate compatible and other versions
    QStringList compatibleVersions;
    QStringList otherVersions;

    for (auto it = imageFiles.constBegin(); it != imageFiles.constEnd(); ++it) {
        if (it.value().contains("DeveloperDiskImage.dmg") &&
            it.value().contains("DeveloperDiskImage.dmg.signature")) {
            QFileInfo dirInfo(it.key());
            QString version = dirInfo.fileName();
            m_availableImages[version] = {
                it.value()["DeveloperDiskImage.dmg"],
                it.value()["DeveloperDiskImage.dmg.signature"]};

            // Determine compatibility
            bool isCompatible = false;
            if (hasConnectedDevice) {
                // Parse version string (e.g., "15.0", "16.1")
                QStringList versionParts = version.split('.');
                if (versionParts.size() >= 1) {
                    bool ok;
                    int imageMajorVersion = versionParts[0].toInt(&ok);
                    if (ok) {
                        // iOS 16+ uses iOS 16 images, earlier versions use
                        // exact or lower version
                        if (deviceMajorVersion >= 16) {
                            isCompatible = (imageMajorVersion == 16);
                        } else {
                            isCompatible =
                                (imageMajorVersion == deviceMajorVersion);
                        }
                    }
                }
            }

            if (isCompatible) {
                compatibleVersions.append(version);
            } else {
                otherVersions.append(version);
            }
        }
    }

    // Sort versions (compatible ones first, then others)
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

    // Create UI items - compatible versions first
    auto createVersionItem = [&](const QString &version, bool isCompatible) {
        auto *itemWidget = new QWidget();
        auto *itemLayout = new QHBoxLayout(itemWidget);

        auto *versionLabel = new QLabel(version);
        if (isCompatible) {
            versionLabel->setStyleSheet(
                "QLabel { font-weight: bold; color: #2E7D32; }");
        }
        itemLayout->addWidget(versionLabel);

        // Add compatibility label
        if (hasConnectedDevice) {
            if (isCompatible) {
                auto *compatLabel = new QLabel(tr("✓ Compatible"));
                compatLabel->setStyleSheet(
                    "QLabel { color: #2E7D32; font-weight: bold; }");
                itemLayout->addWidget(compatLabel);
            } else {
                auto *incompatLabel = new QLabel(tr("⚠ Not recommended"));
                incompatLabel->setStyleSheet("QLabel { color: #F57C00; }");
                itemLayout->addWidget(incompatLabel);
            }
        }

        QString versionPath = QDir(m_downloadPath).filePath(version);
        bool exists = QDir(versionPath).exists();

        if (exists) {
            itemLayout->addWidget(new QLabel(tr("(already exists)")));
        }

        itemLayout->addStretch();

        auto *progressBar = new QProgressBar();
        progressBar->setVisible(false);
        itemLayout->addWidget(progressBar);

        auto *downloadButton =
            new QPushButton(exists ? tr("Re-download") : tr("Download"));
        downloadButton->setProperty("version", version);
        connect(downloadButton, &QPushButton::clicked, this,
                &DevDiskImagesWidget::onDownloadButtonClicked);
        itemLayout->addWidget(downloadButton);

        auto *listItem = new QListWidgetItem(m_imageListWidget);
        listItem->setSizeHint(itemWidget->sizeHint());
        m_imageListWidget->addItem(listItem);
        m_imageListWidget->setItemWidget(listItem, itemWidget);
    };

    // Add compatible versions first
    for (const QString &version : compatibleVersions) {
        createVersionItem(version, true);
    }

    // Add separator if we have both compatible and other versions
    if (!compatibleVersions.isEmpty() && !otherVersions.isEmpty()) {
        auto *separatorItem = new QListWidgetItem(m_imageListWidget);
        auto *separatorWidget = new QWidget();
        auto *separatorLayout = new QHBoxLayout(separatorWidget);
        auto *separatorLabel = new QLabel(tr("Other versions"));
        separatorLabel->setStyleSheet(
            "QLabel { font-weight: bold; color: #757575; margin: 10px 0; }");
        separatorLayout->addWidget(separatorLabel);
        separatorItem->setSizeHint(separatorWidget->sizeHint());
        m_imageListWidget->addItem(separatorItem);
        m_imageListWidget->setItemWidget(separatorItem, separatorWidget);
    }

    // Add other versions
    for (const QString &version : otherVersions) {
        createVersionItem(version, false);
    }
}

void DevDiskImagesWidget::onDownloadButtonClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    QString version = button->property("version").toString();

    QString versionPath = QDir(m_downloadPath).filePath(version);
    if (QDir(versionPath).exists()) {
        auto reply = QMessageBox::question(
            this, tr("Confirm Overwrite"),
            tr("Directory '%1' already exists. Do you want to overwrite it?")
                .arg(version),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
    }

    startDownload(version);
}

void DevDiskImagesWidget::startDownload(const QString &version)
{
    if (!m_availableImages.contains(version))
        return;

    // Find the button and progress bar for this version
    QPushButton *downloadButton = nullptr;
    QProgressBar *progressBar = nullptr;
    for (int i = 0; i < m_imageListWidget->count(); ++i) {
        auto *item = m_imageListWidget->item(i);
        auto *widget = m_imageListWidget->itemWidget(item);
        auto *button = widget->findChild<QPushButton *>();
        if (button && button->property("version") == version) {
            downloadButton = button;
            progressBar = widget->findChild<QProgressBar *>();
            break;
        }
    }

    if (!downloadButton || !progressBar)
        return;

    downloadButton->setEnabled(false);
    progressBar->setVisible(true);
    progressBar->setValue(0);

    QString targetDir = QDir(m_downloadPath).filePath(version);
    if (!QDir().mkpath(targetDir)) {
        QMessageBox::critical(
            this, tr("Error"),
            tr("Could not create directory: %1").arg(targetDir));
        downloadButton->setEnabled(true);
        progressBar->setVisible(false);
        return;
    }

    auto *downloadItem = new DownloadItem();
    downloadItem->version = version;
    downloadItem->progressBar = progressBar;
    downloadItem->downloadButton = downloadButton;

    QString dmgPath = m_availableImages[version].first;
    QString sigPath = m_availableImages[version].second;

    QUrl dmgUrl("https://raw.githubusercontent.com/mspvirajpatel/"
                "Xcode_Developer_Disk_Images/master/" +
                dmgPath);
    QNetworkRequest dmgRequest(dmgUrl);
    downloadItem->dmgReply = m_networkManager->get(dmgRequest);
    connect(downloadItem->dmgReply, &QNetworkReply::downloadProgress, this,
            &DevDiskImagesWidget::onDownloadProgress);
    connect(downloadItem->dmgReply, &QNetworkReply::finished, this,
            &DevDiskImagesWidget::onFileDownloadFinished);

    QUrl sigUrl("https://raw.githubusercontent.com/mspvirajpatel/"
                "Xcode_Developer_Disk_Images/master/" +
                sigPath);
    QNetworkRequest sigRequest(sigUrl);
    downloadItem->sigReply = m_networkManager->get(sigRequest);
    connect(downloadItem->sigReply, &QNetworkReply::downloadProgress, this,
            &DevDiskImagesWidget::onDownloadProgress);
    connect(downloadItem->sigReply, &QNetworkReply::finished, this,
            &DevDiskImagesWidget::onFileDownloadFinished);

    m_activeDownloads[downloadItem->dmgReply] = downloadItem;
    m_activeDownloads[downloadItem->sigReply] = downloadItem;
}

void DevDiskImagesWidget::onDownloadProgress(qint64 bytesReceived,
                                             qint64 bytesTotal)
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

    item->totalReceived = item->dmgReceived + item->sigReceived;

    if (item->totalSize > 0) {
        item->progressBar->setValue((item->totalReceived * 100) /
                                    item->totalSize);
    }
}

void DevDiskImagesWidget::onFileDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || !m_activeDownloads.contains(reply))
        return;

    auto *item = m_activeDownloads[reply];
    m_activeDownloads.remove(reply);

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::critical(this, tr("Download Error"),
                              tr("Failed to download %1: %2")
                                  .arg(reply->url().path())
                                  .arg(reply->errorString()));

        if (reply == item->dmgReply && item->sigReply)
            item->sigReply->abort();
        if (reply == item->sigReply && item->dmgReply)
            item->dmgReply->abort();

        item->downloadButton->setEnabled(true);
        item->downloadButton->setText(tr("Retry"));
        item->progressBar->setVisible(false);

        if (m_activeDownloads.key(item) == nullptr) {
            delete item;
        }
        reply->deleteLater();
        return;
    }

    QString path = QUrl::fromPercentEncoding(reply->url().path().toUtf8());
    QFileInfo fileInfo(path);
    QString filename = fileInfo.fileName();
    QString targetPath =
        QDir(QDir(m_downloadPath).filePath(item->version)).filePath(filename);

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("File Error"),
                              tr("Could not save file: %1").arg(targetPath));
    } else {
        file.write(reply->readAll());
        file.close();
    }

    reply->deleteLater();

    if (m_activeDownloads.key(item) == nullptr) { // Both files downloaded
        item->downloadButton->setText(tr("Downloaded"));
        item->downloadButton->setEnabled(false);
        item->progressBar->setValue(100);
        item->progressBar->setVisible(false);
        delete item;
    }
}

void DevDiskImagesWidget::updateDeviceList()
{
    auto devices = AppContext::sharedInstance()->getAllDevices();
    qDebug() << "devdiskwidget devices:" << devices.size();
    QString currentUdid = "";
    if (m_deviceComboBox->count() > 0 &&
        m_deviceComboBox->currentIndex() >= 0) {
        currentUdid = m_deviceComboBox->currentData().toString();
    }

    // Temporarily disconnect to avoid triggering onDeviceSelectionChanged
    // multiple times
    disconnect(m_deviceComboBox,
               QOverload<int>::of(&QComboBox::currentIndexChanged), this,
               &DevDiskImagesWidget::onDeviceSelectionChanged);

    m_deviceComboBox->clear();

    int newIndex = -1;
    for (int i = 0; i < devices.size(); ++i) {
        auto *device = devices.at(i);
        m_deviceComboBox->addItem(
            QString("%1 (%2)")
                .arg(QString::fromStdString(device->deviceInfo.deviceName))
                .arg(QString::fromStdString(device->udid)),
            QString::fromStdString(device->udid));
        if (QString().fromStdString((device->udid)) == currentUdid) {
            newIndex = i;
        }
    }

    if (newIndex != -1) {
        m_deviceComboBox->setCurrentIndex(newIndex);
        m_currentDevice = devices.at(newIndex);
    } else if (!devices.isEmpty()) {
        // If no previous device was selected but devices are available, select
        // the first one
        m_deviceComboBox->setCurrentIndex(0);
        m_currentDevice = devices.at(0);
    } else {
        m_currentDevice = nullptr;
    }

    // Reconnect the signal
    connect(m_deviceComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DevDiskImagesWidget::onDeviceSelectionChanged);

    qDebug() << "devdiskwidget device:" << m_deviceComboBox->currentText();
    qDebug() << "devdiskwidget Current device:"
             << (m_currentDevice
                     ? m_currentDevice->deviceInfo.deviceName.c_str()
                     : "None");

    // Refresh the UI with the updated device information
    if (!m_imageListJsonData.isEmpty()) {
        parseAndDisplayImages(m_imageListJsonData);
    }
}

void DevDiskImagesWidget::onMountButtonClicked()
{
    if (m_deviceComboBox->currentIndex() < 0) {
        QMessageBox::warning(
            this, tr("No Device"),
            tr("Please select a device to mount the image on."));
        return;
    }

    auto *currentItem = m_imageListWidget->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, tr("No Image Selected"),
                             tr("Please select a disk image to mount."));
        return;
    }

    auto *widget = m_imageListWidget->itemWidget(currentItem);
    auto *button = widget->findChild<QPushButton *>();
    if (!button)
        return;

    QString version = button->property("version").toString();

    mountImage(version);
}

void DevDiskImagesWidget::mountImage(const QString &version)
{
    QString udid = m_deviceComboBox->currentData().toString();
    if (udid.isEmpty()) {
        QMessageBox::warning(this, tr("No Device"),
                             tr("Please select a device."));
        return;
    }

    // TODO: add a refresh button
    QString versionPath = QDir(m_downloadPath).filePath(version);
    if (!QDir(versionPath).exists()) {
        QMessageBox::warning(
            this, tr("Image Not Found"),
            tr("The selected disk image for version %1 is not downloaded. "
               "Please download it first.")
                .arg(version));
        return;
    }

    QString dmgPath = QDir(versionPath).filePath("DeveloperDiskImage.dmg");
    QString sigPath =
        QDir(versionPath).filePath("DeveloperDiskImage.dmg.signature");

    if (!QFile::exists(dmgPath) || !QFile::exists(sigPath)) {
        QMessageBox::warning(
            this, tr("Image Files Missing"),
            tr("Image files are missing in %1. Please re-download.")
                .arg(versionPath));
        return;
    }

    m_mountButton->setEnabled(false);
    m_mountButton->setText(tr("Mounting..."));

    bool success = mount_dev_image(udid.toUtf8().constData(),
                                   versionPath.toUtf8().constData());

    m_mountButton->setEnabled(true);
    m_mountButton->setText(tr("Mount"));

    if (success) {
        QMessageBox::information(this, tr("Success"),
                                 tr("Image mounted successfully on %1.")
                                     .arg(m_deviceComboBox->currentText()));
    } else {
        QMessageBox::critical(this, tr("Mount Failed"),
                              tr("Failed to mount image on %1.")
                                  .arg(m_deviceComboBox->currentText()));
    }
}

void DevDiskImagesWidget::changeDownloadDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Download Directory"), m_downloadPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty() && dir != m_downloadPath) {
        m_downloadPath = dir;
        m_downloadPathEdit->setText(m_downloadPath);
        if (!m_imageListJsonData.isEmpty()) {
            parseAndDisplayImages(m_imageListJsonData);
        }
    }
}

void DevDiskImagesWidget::closeEvent(QCloseEvent *event)
{
    if (!m_activeDownloads.isEmpty()) {
        auto reply = QMessageBox::question(
            this, tr("Downloads in Progress"),
            tr("There are %1 download(s) in progress. Do you really want to "
               "close and cancel all downloads?")
                .arg(m_activeDownloads.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }

        // Cancel all active downloads
        for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end();
             ++it) {
            QNetworkReply *reply = it.key();
            if (reply) {
                reply->abort();
            }
        }
    }

    event->accept();
}