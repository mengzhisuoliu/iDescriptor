#include "devdiskimageswidget.h"
#include "appcontext.h"
#include "devdiskmanager.h"
#include "iDescriptor.h"
#include "settingsmanager.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
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
#include <string>

// TODO:sometimes non authentic cables do not work with img mounting

DevDiskImagesWidget::DevDiskImagesWidget(iDescriptorDevice *device,
                                         QWidget *parent)
    : QWidget{parent}, m_currentDevice(device)
{
    setupUi();

    // Connect to manager signals
    // TODO: can prevent race condition ?
    connect(DevDiskManager::sharedInstance(), &DevDiskManager::imageListFetched,
            this, &DevDiskImagesWidget::onImageListFetched);

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
    setWindowTitle("Developer Disk Images - iDescriptor");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *mountLayout = new QHBoxLayout();
    mountLayout->addWidget(new QLabel("Device:"));
    m_deviceComboBox = new QComboBox(this);
    mountLayout->addWidget(m_deviceComboBox);
    m_mountButton = new QPushButton("Mount", this);
    m_check_mountedButton = new QPushButton("Check Mounted", this);
    connect(m_mountButton, &QPushButton::clicked, this,
            &DevDiskImagesWidget::onMountButtonClicked);
    connect(m_check_mountedButton, &QPushButton::clicked, this,
            &DevDiskImagesWidget::checkMountedImage);
    mountLayout->setContentsMargins(10, 10, 10, 10);
    mountLayout->addWidget(m_mountButton);
    mountLayout->addWidget(m_check_mountedButton);
    layout->addLayout(mountLayout);

    auto *pathLayout = new QHBoxLayout();
    // main path/info row (no shadow)
    auto *pathWidget = new QWidget();
    pathWidget->setLayout(pathLayout);
    pathLayout->addWidget(
        new QLabel("You can change the download path from settings :"));
    QPushButton *openSettingsButton = new QPushButton("Open Settings");
    pathLayout->addWidget(openSettingsButton);
    connect(openSettingsButton, &QPushButton::clicked, this, [this]() {
        SettingsManager::sharedInstance()->showSettingsDialog();
    });
    pathLayout->setContentsMargins(10, 10, 10, 10);
    layout->addWidget(pathWidget);

    // thin centered bottom line + shadow (shadow only applied to this line)
    QWidget *lineContainer = new QWidget();
    QHBoxLayout *lineLayout = new QHBoxLayout(lineContainer);
    lineLayout->setContentsMargins(0, 0, 0, 0); // adjust centering / width
    lineLayout->setSpacing(0);

    QWidget *innerLine = new QWidget();
    innerLine->setFixedHeight(2); // thickness of the visible border
    innerLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    innerLine->setStyleSheet("background-color: #363d32;");
    innerLine->setLayout(new QHBoxLayout());
    innerLine->layout()->setContentsMargins(0, 0, 0, 0);
    innerLine->layout()->setSpacing(0);

    // apply shadow only to the thin line so shadow appears only under bottom
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(0, 0, 0, 30));
    shadow->setOffset(0, 6);
    innerLine->setGraphicsEffect(shadow);

    // If you want the line to be shorter than full width, give it a max width:
    // innerLine->setMaximumWidth( int(width * 0.8) ); // or manage in
    // resizeEvent

    lineLayout->addStretch();
    lineLayout->addWidget(innerLine);
    lineLayout->addStretch();
    layout->addWidget(lineContainer);

    m_stackedWidget = new QStackedWidget(this);
    layout->addWidget(m_stackedWidget);

    m_statusLabel = new QLabel("Fetching image list...");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_stackedWidget->addWidget(m_statusLabel);

    m_imageListWidget = new QListWidget(this);
    m_imageListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_imageListWidget->setStyleSheet(
        "QListWidget { background: transparent; border: none; }");
    m_imageListWidget->viewport()->setStyleSheet("background: transparent;");

    m_stackedWidget->addWidget(m_imageListWidget);

    displayImages();
    if (DevDiskManager::sharedInstance()->isImageListReady()) {
        m_stackedWidget->setCurrentWidget(m_imageListWidget);
    }
}

void DevDiskImagesWidget::fetchImages()
{
    m_stackedWidget->setCurrentWidget(m_statusLabel);
    m_statusLabel->setText("Fetching image list...");
    // DevDiskManager::sharedInstance()->fetchImageList();
}

void DevDiskImagesWidget::onImageListFetched(bool success,
                                             const QString &errorMessage)
{

    qDebug() << "Image list fetched successfully";
    if (!success) {
        m_statusLabel->setText(
            QString("Error fetching image list: %1").arg(errorMessage));
        return;
    }

    displayImages();
    m_stackedWidget->setCurrentWidget(m_imageListWidget);
}

void DevDiskImagesWidget::onDeviceSelectionChanged(int index)
{
    if (index < 0 ||
        index >= AppContext::sharedInstance()->getAllDevices().size())
        return;

    m_currentDevice = AppContext::sharedInstance()->getAllDevices()[index];
    displayImages();
}

void DevDiskImagesWidget::displayImages()
{
    m_imageListWidget->clear();

    // Get device version for compatibility checking
    int deviceMajorVersion = 0;
    int deviceMinorVersion = 0;
    bool hasConnectedDevice = false;

    if (m_currentDevice && m_currentDevice->device) {
        unsigned int device_version =
            idevice_get_device_version(m_currentDevice->device);
        deviceMajorVersion = (device_version >> 16) & 0xFF;
        deviceMinorVersion = (device_version >> 8) & 0xFF;
        hasConnectedDevice = true;
    }

    qDebug() << "Device version:" << deviceMajorVersion << "."
             << deviceMinorVersion << "displayImages";
    // Parse images using manager
    GetImagesSortedFinalResult sortedResult =
        DevDiskManager::sharedInstance()->parseImageList(
            deviceMajorVersion, deviceMinorVersion, m_mounted_sig,
            m_mounted_sig_len);

    auto compatibleImages = sortedResult.compatibleImages;
    auto otherImages = sortedResult.otherImages;

    qDebug() << "Compatible images:" << compatibleImages.size();
    qDebug() << "Other images:" << otherImages.size();

    // Create UI items - compatible versions first
    auto createVersionItem = [&](const ImageInfo &info, bool isCompatible) {
        auto *itemWidget = new QWidget();
        auto *itemLayout = new QHBoxLayout(itemWidget);

        auto *versionLabel = new QLabel(info.version);
        if (isCompatible) {
            versionLabel->setStyleSheet(
                "QLabel { font-weight: bold; color: #2E7D32; }");
        }
        itemLayout->addWidget(versionLabel);

        // Add status labels
        if (hasConnectedDevice) {
            if (isCompatible) {
                if (info.isMounted) {
                    auto *mountedLabel = new QLabel("✓ Mounted");
                    mountedLabel->setStyleSheet(
                        "QLabel { color: #1565C0; font-weight: bold; }");
                    itemLayout->addWidget(mountedLabel);
                }
            } else {
                auto *incompatLabel = new QLabel("⚠ Not compatible");
                incompatLabel->setStyleSheet(
                    "QLabel { color: #F57C00; margin-left: 10px; font-weight: "
                    "bold; }");
                itemLayout->addWidget(incompatLabel);
            }
        }

        itemLayout->addStretch();

        auto *progressBar = new QProgressBar();
        progressBar->setVisible(false);
        itemLayout->addWidget(progressBar);

        auto *downloadButton =
            new QPushButton(info.isDownloaded ? "Re-download" : "Download");
        downloadButton->setProperty("version", info.version);
        connect(downloadButton, &QPushButton::clicked, this,
                &DevDiskImagesWidget::onDownloadButtonClicked);
        itemLayout->addWidget(downloadButton);

        auto *listItem = new QListWidgetItem(m_imageListWidget);
        listItem->setSizeHint(itemWidget->sizeHint());
        m_imageListWidget->addItem(listItem);
        m_imageListWidget->setItemWidget(listItem, itemWidget);
    };

    // Add compatible versions first
    for (const auto &info : compatibleImages) {
        createVersionItem(info, true);
    }

    // Add separator if we have both compatible and other versions
    if (!compatibleImages.isEmpty() && !otherImages.isEmpty()) {
        auto *separatorItem = new QListWidgetItem(m_imageListWidget);
        auto *separatorWidget = new QWidget();
        auto *separatorLayout = new QHBoxLayout(separatorWidget);
        auto *separatorLabel = new QLabel("Other versions");
        separatorLabel->setStyleSheet(
            "QLabel { font-weight: bold; color: #757575; margin: 10px 0; }");
        separatorLayout->addWidget(separatorLabel);
        separatorItem->setSizeHint(separatorWidget->sizeHint());
        m_imageListWidget->addItem(separatorItem);
        m_imageListWidget->setItemWidget(separatorItem, separatorWidget);
    }

    // Add other versions
    for (const auto &info : otherImages) {
        createVersionItem(info, false);
    }

    // Show device info if available
    if (hasConnectedDevice) {
        QString deviceVersion =
            QString("%1.%2").arg(deviceMajorVersion).arg(deviceMinorVersion);
        m_statusLabel->setText(
            QString("Connected device: iOS %1 - Compatible images shown at top")
                .arg(deviceVersion));
    }
}

void DevDiskImagesWidget::onDownloadButtonClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    QString version = button->property("version").toString();

    QString versionPath =
        QDir(SettingsManager::sharedInstance()->devdiskimgpath())
            .filePath(version);
    if (QDir(versionPath).exists()) {
        auto reply = QMessageBox::question(
            this, "Confirm Overwrite",
            QString(
                "Directory '%1' already exists. Do you want to overwrite it?")
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

    QString targetDir =
        QDir(SettingsManager::sharedInstance()->devdiskimgpath())
            .filePath(version);
    if (!QDir().mkpath(targetDir)) {
        QMessageBox::critical(
            this, "Error",
            QString("Could not create directory: %1").arg(targetDir));
        downloadButton->setEnabled(true);
        progressBar->setVisible(false);
        return;
    }

    auto *downloadItem = new DownloadItem();
    downloadItem->version = version;
    downloadItem->progressBar = progressBar;
    downloadItem->downloadButton = downloadButton;

    auto replies = DevDiskManager::sharedInstance()->downloadImage(version);
    downloadItem->dmgReply = replies.first;
    downloadItem->sigReply = replies.second;

    if (!downloadItem->dmgReply || !downloadItem->sigReply) {
        delete downloadItem;
        downloadButton->setEnabled(true);
        progressBar->setVisible(false);
        return;
    }

    connect(downloadItem->dmgReply, &QNetworkReply::downloadProgress, this,
            &DevDiskImagesWidget::onDownloadProgress);
    connect(downloadItem->dmgReply, &QNetworkReply::finished, this,
            &DevDiskImagesWidget::onFileDownloadFinished);
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

// TODO: file saving should be in manager
void DevDiskImagesWidget::onFileDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || !m_activeDownloads.contains(reply))
        return;

    auto *item = m_activeDownloads[reply];
    m_activeDownloads.remove(reply);

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::critical(this, "Download Error",
                              QString("Failed to download %1: %2")
                                  .arg(reply->url().path())
                                  .arg(reply->errorString()));

        if (reply == item->dmgReply && item->sigReply)
            item->sigReply->abort();
        if (reply == item->sigReply && item->dmgReply)
            item->dmgReply->abort();

        item->downloadButton->setEnabled(true);
        item->downloadButton->setText("Retry");
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
        QDir(QDir(SettingsManager::sharedInstance()->devdiskimgpath())
                 .filePath(item->version))
            .filePath(filename);

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(
            this, "File Error",
            QString("Could not save file: %1").arg(targetPath));
    } else {
        file.write(reply->readAll());
        file.close();
    }

    reply->deleteLater();

    if (m_activeDownloads.key(item) == nullptr) { // Both files downloaded
        item->downloadButton->setText("Downloaded");
        item->downloadButton->setEnabled(false);
        item->progressBar->setValue(100);
        item->progressBar->setVisible(false);
        delete item;
    }
}

void DevDiskImagesWidget::updateDeviceList()
{
    auto devices = AppContext::sharedInstance()->getAllDevices();
    QString currentUdid = "";
    if (m_deviceComboBox->count() > 0 &&
        m_deviceComboBox->currentIndex() >= 0) {
        currentUdid = m_deviceComboBox->currentData().toString();
    }

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
    }
    displayImages();
}

void DevDiskImagesWidget::onMountButtonClicked()
{
    if (m_deviceComboBox->currentIndex() < 0) {
        QMessageBox::warning(this, "No Device",
                             "Please select a device to mount the image on.");
        return;
    }

    auto *currentItem = m_imageListWidget->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "No Image Selected",
                             "Please select a disk image to mount.");
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
        QMessageBox::warning(this, "No Device", "Please select a device.");
        return;
    }

    if (!DevDiskManager::sharedInstance()->isImageDownloaded(
            version, SettingsManager::sharedInstance()->devdiskimgpath())) {
        QMessageBox::warning(
            this, "Image Not Found",
            QString("The selected disk image for version %1 is not downloaded. "
                    "Please download it first.")
                .arg(version));
        return;
    }

    m_mountButton->setEnabled(false);
    m_mountButton->setText("Mounting...");

    bool success = DevDiskManager::sharedInstance()->mountImage(version, udid);

    m_mountButton->setEnabled(true);
    m_mountButton->setText("Mount");

    if (success) {
        QMessageBox::information(this, "Success",
                                 QString("Image mounted successfully on %1.")
                                     .arg(m_deviceComboBox->currentText()));
        displayImages(); // Refresh to show mounted status
    } else {
        QMessageBox::critical(this, "Mount Failed",
                              QString("Failed to mount image on %1.")
                                  .arg(m_deviceComboBox->currentText()));
    }
}

void DevDiskImagesWidget::closeEvent(QCloseEvent *event)
{
    if (!m_activeDownloads.isEmpty()) {
        auto reply = QMessageBox::question(
            this, "Downloads in Progress",
            QString(
                "There are %1 download(s) in progress. Do you really want to "
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

// Toolbox clicked: "Developer Disk Images"
// terminate called after throwing an instance of 'std::logic_error'
//   what():  basic_string: construction from null is not valid

void DevDiskImagesWidget::checkMountedImage()
{
    if (m_deviceComboBox->currentIndex() < 0) {
        QMessageBox::warning(
            this, "No Device",
            "Please select a device to check the mounted image.");
        return;
    }

    GetMountedImageResult result =
        DevDiskManager::sharedInstance()->getMountedImage(
            m_currentDevice->udid.c_str());

    qDebug() << "checkMountedImage result:" << result.success
             << result.message.c_str() << QString::fromStdString(result.output);

    if (result.success) {

        m_mounted_sig = strdup(result.output.c_str());
        m_mounted_sig_len = result.output.size();
        displayImages(); // Refresh to show mounted status
        return;
    }

    QMessageBox::information(this, "Something went wrong",
                             result.message.c_str());
    //     get_mounted_image(m_currentDevice->udid.c_str());

    // plist_t sig_array_node =
    //     plist_dict_get_item(result.output, "ImageSignature");

    // if (result.success == false || sig_array_node == NULL) {
    //     QMessageBox::information(
    //         this, "Locked",
    //         "The device is locked. Please unlock it and try again.");
    //     return;
    // }

    // char *mounted_sig = nullptr;
    // uint64_t mounted_sig_len = 0;

    // if (sig_array_node && plist_get_node_type(sig_array_node) == PLIST_ARRAY
    // &&
    //     plist_array_get_size(sig_array_node) > 0) {
    //     plist_t sig_data_node = plist_array_get_item(sig_array_node, 0);
    //     if (sig_data_node && plist_get_node_type(sig_data_node) ==
    //     PLIST_DATA) {
    //         plist_get_data_val(sig_data_node, &mounted_sig,
    //         &mounted_sig_len);
    //     }
    // }

    // auto compatibleImages =
    //     DevDiskManager::sharedInstance()->getCompatibleImages();
    // for (const auto &info : compatibleImages) {
    //     if (info.isMounted) {
    //         displayImages(); // Refresh to show mounted status
    //         return;
    //     }
    // }

    // QMessageBox::information(this, "Not Mounted",
    //                          "The device has no mounted images.");
}
