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

#include "devdiskimagehelper.h"
#include "devdiskmanager.h"
#include "qprocessindicator.h"
#include "settingsmanager.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

DevDiskImageHelper::DevDiskImageHelper(iDescriptorDevice *device,
                                       QWidget *parent)
    : QDialog(parent), m_device(device), m_isDownloading(false),
      m_isMounting(false)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Developer Disk Image - iDescriptor");
    setupUI();

    connect(this, &QDialog::accepted, this,
            [this]() { emit mountingCompleted(true); });
    connect(this, &QDialog::rejected, this,
            [this]() { emit mountingCompleted(false); });
}

void DevDiskImageHelper::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Loading indicator
    auto *indicatorLayout = new QHBoxLayout();
    indicatorLayout->addStretch();
    m_loadingIndicator = new QProcessIndicator();
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(64, 32);
    indicatorLayout->addWidget(m_loadingIndicator);
    indicatorLayout->addStretch();
    mainLayout->addLayout(indicatorLayout);

    // Status label
    m_statusLabel = new QLabel("Checking developer disk image...");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    // Button layout
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_mountButton = new QPushButton("Mount");
    m_mountButton->setDefault(true);
    m_mountButton->setVisible(false);
    connect(m_mountButton, &QPushButton::clicked, this,
            &DevDiskImageHelper::onMountButtonClicked);
    buttonLayout->addWidget(m_mountButton);

    m_retryButton = new QPushButton("Retry");
    m_retryButton->setVisible(false);
    connect(m_retryButton, &QPushButton::clicked, this,
            &DevDiskImageHelper::onRetryButtonClicked);
    buttonLayout->addWidget(m_retryButton);

    m_cancelButton = new QPushButton("Cancel");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    setMinimumWidth(400);
    setModal(true);
}

void DevDiskImageHelper::start()
{
    m_loadingIndicator->start();
    showStatus("Please wait...");

    unsigned int device_version = idevice_get_device_version(m_device->device);
    unsigned int deviceMajorVersion = (device_version >> 16) & 0xFF;
    unsigned int deviceMinorVersion = (device_version >> 8) & 0xFF;

    // FIXME:we dont have developer disk images for ios 6 and below
    if (deviceMajorVersion > 5) {
        const bool isMountAvailable =
            DevDiskManager::sharedInstance()->downloadCompatibleImage(
                m_device, [this](bool success) {
                    if (success) {
                        checkAndMount();
                    } else {
                        finishWithError("Failed to download compatible image.");
                    }
                });
        qDebug() << "isMountAvailable:" << isMountAvailable;
        if (!isMountAvailable) {
            finishWithError("Failed to download compatible image.");
        }
    } else {
        finishWithSuccess();
        return;
    }
}

void DevDiskImageHelper::checkAndMount()
{
    GetMountedImageResult result =
        DevDiskManager::sharedInstance()->getMountedImage(
            m_device->udid.c_str());
    qDebug() << "checkAndMount result:" << result.success
             << result.message.c_str() << QString::fromStdString(result.sig);
    if (!result.success) {
        showRetryUI(QString::fromStdString(result.message));
        return;
    }

    // If image is already mounted
    if (!result.sig.empty()) {
        finishWithSuccess();
        return;
    }

    onMountButtonClicked();
}

void DevDiskImageHelper::onMountButtonClicked()
{
    QString path = SettingsManager::sharedInstance()->mkDevDiskImgPath();
    m_mountButton->setVisible(false);
    m_loadingIndicator->start();
    m_isMounting = true;

    // Check if we need to download first
    unsigned int device_version = idevice_get_device_version(m_device->device);
    unsigned int deviceMajorVersion = (device_version >> 16) & 0xFF;
    unsigned int deviceMinorVersion = (device_version >> 8) & 0xFF;

    QList<ImageInfo> images = DevDiskManager::sharedInstance()->parseImageList(
        path, deviceMajorVersion, deviceMinorVersion, "", 0);

    // Check if compatible image is downloaded
    bool hasDownloadedImage = false;
    QString versionToMount;

    for (const ImageInfo &info : images) {
        if (info.compatibility == ImageCompatibility::Compatible ||
            info.compatibility == ImageCompatibility::MaybeCompatible) {
            if (info.isDownloaded) {
                hasDownloadedImage = true;
                versionToMount = info.version;
                break;
            }
        }
    }

    if (hasDownloadedImage) {
        // Mount directly
        showStatus("Mounting developer disk image...");

        mobile_image_mounter_error_t err =
            DevDiskManager::sharedInstance()->mountImage(versionToMount,
                                                         m_device);

        m_isMounting = false;
        if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
            showStatus("Developer disk image mounted successfully");
            finishWithSuccess();
        } else if (err == MOBILE_IMAGE_MOUNTER_E_DEVICE_LOCKED) {
            showRetryUI(
                "Device is locked. Please unlock your device and try again.");
        } else {
            showRetryUI("Failed to mount developer disk image.\n"
                        "Please ensure:\n"
                        "• Device is unlocked\n"
                        "• Using a genuine cable\n"
                        "• Developer mode is enabled (iOS 16+)");
        }
    } else {
        // Need to download first
        showStatus(
            "Downloading developer disk image...\nThis may take a moment.");
        m_isDownloading = true;

        // Connect to download signals
        connect(DevDiskManager::sharedInstance(),
                &DevDiskManager::imageDownloadFinished, this,
                &DevDiskImageHelper::onImageDownloadFinished,
                Qt::UniqueConnection);

        // Find version to download
        for (const ImageInfo &info : images) {
            if (info.compatibility == ImageCompatibility::Compatible ||
                info.compatibility == ImageCompatibility::MaybeCompatible) {
                m_downloadingVersion = info.version;
                break;
            }
        }
    }
}

void DevDiskImageHelper::onImageDownloadFinished(const QString &version,
                                                 bool success,
                                                 const QString &errorMessage)
{
    if (!m_isDownloading || version != m_downloadingVersion) {
        return;
    }

    m_isDownloading = false;

    if (!success) {
        showRetryUI("Failed to download developer disk image:\n" +
                    errorMessage);
        return;
    }

    // Download successful, now mount
    showStatus("Download complete. Mounting...");

    mobile_image_mounter_error_t err =
        DevDiskManager::sharedInstance()->mountImage(version, m_device);

    if (err == MOBILE_IMAGE_MOUNTER_E_SUCCESS) {
        showStatus("Developer disk image mounted successfully");
        finishWithSuccess();
    } else if (err == MOBILE_IMAGE_MOUNTER_E_DEVICE_LOCKED) {
        showRetryUI(
            "Device is locked. Please unlock your device and try again.");
    } else {
        showRetryUI(
            "Failed to mount developer disk image.\n"
            "Please ensure the device is unlocked and using a genuine cable.");
    }
}

void DevDiskImageHelper::showRetryUI(const QString &errorMessage)
{
    m_loadingIndicator->stop();
    showStatus(errorMessage, true);
    m_mountButton->setVisible(false);
    m_retryButton->setVisible(true);
    m_cancelButton->setText("Close");
}

void DevDiskImageHelper::onRetryButtonClicked()
{
    m_retryButton->setVisible(false);
    m_cancelButton->setText("Cancel");
    start();
}

void DevDiskImageHelper::showStatus(const QString &message, bool isError)
{
    m_statusLabel->setText(message);

    show();
}

void DevDiskImageHelper::finishWithSuccess()
{
    m_loadingIndicator->stop();
    accept();
}

void DevDiskImageHelper::finishWithError(const QString &errorMessage)
{
    m_loadingIndicator->stop();
    showStatus(errorMessage, true);
}
