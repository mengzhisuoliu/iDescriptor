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
#include "appcontext.h"
#include "devdiskmanager.h"
#include "settingsmanager.h"
#include "zloadingwidget.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

DevDiskImageHelper::DevDiskImageHelper(
    const std::shared_ptr<iDescriptorDevice> device, QWidget *parent)
    : QDialog(parent), m_device(device)
{
    setAttribute(Qt::WA_DeleteOnClose);
#ifdef WIN32
    setupWinWindow(this);
#endif
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

    // ZLoadingWidget handles the spinner + state switching
    m_loadingWidget = new ZLoadingWidget(true, this);
    mainLayout->addWidget(m_loadingWidget);

    // Custom error layout: message + Retry
    auto *errorLayout = new QHBoxLayout();
    errorLayout->addStretch();

    m_statusLabel = new QLabel("An error occurred.");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    errorLayout->addWidget(m_statusLabel);

    m_retryButton = new QPushButton("Retry");
    connect(m_retryButton, &QPushButton::clicked, this,
            &DevDiskImageHelper::onRetryButtonClicked);
    errorLayout->addWidget(m_retryButton);

    errorLayout->addStretch();

    // Register custom error layout with ZLoadingWidget
    m_loadingWidget->setupErrorWidget(errorLayout);

    // Bottom button row (Cancel / Close)
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

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
    if (m_cancelButton) {
        m_cancelButton->setText("Cancel");
    }
    if (m_loadingWidget) {
        m_loadingWidget->showLoading();
    }
    showStatus("Please wait...");

    unsigned int deviceMajorVersion =
        m_device->deviceInfo.parsedDeviceVersion.major;

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
            finishWithError(
                "There is no compatible developer disk image available for " +
                QString::number(deviceMajorVersion) + ".");
        }
    } else {
        showStatus("Developer disk image is not available for iOS version " +
                       QString::number(deviceMajorVersion) +
                       ". Please use a device with iOS 6 or above.",
                   true);
        return;
    }
}

void DevDiskImageHelper::checkAndMount()
{
    connect(
        m_device->service_manager,
        &CXX::ServiceManager::mounted_image_retrieved, this,
        [this](QByteArray signature, u_int64_t sig_length) {
            if (!signature.isEmpty() || sig_length > 0) {
                qDebug()
                    << "Developer disk image already mounted with signature:"
                    << "length:" << sig_length << "signature:" << signature;
                finishWithSuccess();
            } else {
                onMountButtonClicked();
            }
        },
        Qt::SingleShotConnection);
    m_device->service_manager->get_mounted_image();
}

void DevDiskImageHelper::onMountButtonClicked()
{
    QString path = SettingsManager::sharedInstance()->mkDevDiskImgPath();

    if (m_loadingWidget) {
        m_loadingWidget->showLoading();
    }

    // Check if we need to download first
    unsigned int deviceMajorVersion =
        m_device->deviceInfo.parsedDeviceVersion.major;
    unsigned int deviceMinorVersion =
        m_device->deviceInfo.parsedDeviceVersion.minor;

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
        // // Mount directly
        m_downloadingVersion = versionToMount;
        showStatus("Mounting developer disk image...");
        onImageDownloadFinished(versionToMount, true, "");
    } else {
        // Need to download first
        showStatus(
            "Downloading developer disk image...\nThis may take a moment.");

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
    if (version != m_downloadingVersion) {
        qDebug() << "Ignoring download finished for version" << version
                 << "expected" << m_downloadingVersion;
        return;
    }

    if (!success) {
        showRetryUI("Failed to download developer disk image:\n" +
                    errorMessage);
        return;
    }

    showStatus("Download complete. Mounting...");

    auto paths = DevDiskManager::sharedInstance()->getPathsForVersion(version);
    qDebug() << "Mounting image with paths:" << paths.first << paths.second;

    // FIXME
    // err->code == DeviceLockedMountErrorCode
    // check for error code
    connect(
        m_device->service_manager, &CXX::ServiceManager::dev_image_mounted,
        this,
        [this](bool success) {
            qDebug() << "[devdiskimagehelper] : Developer disk image "
                        "mount result:"
                     << success;
            if (success) {
                qDebug() << "[devdiskimagehelper] : Developer disk image "
                            "mounted successfully.";
                finishWithSuccess(true);
            } else {
                qDebug() << "[devdiskimagehelper] : Failed to mount developer "
                            "disk image.";
                showRetryUI(
                    "Failed to mount developer disk image.\n"
                    "Please ensure the device is unlocked and using a genuine "
                    "cable.");
            }
        },
        Qt::SingleShotConnection);

    m_device->service_manager->mount_dev_image(paths.first, paths.second);
}

void DevDiskImageHelper::showRetryUI(const QString &errorMessage)
{
    if (m_statusLabel) {
        m_statusLabel->setText(errorMessage);
    }
    if (m_loadingWidget) {
        m_loadingWidget->showError();
    }
    if (m_cancelButton) {
        m_cancelButton->setText("Close");
    }
}

void DevDiskImageHelper::onRetryButtonClicked()
{
    if (m_cancelButton) {
        m_cancelButton->setText("Cancel");
    }
    if (m_loadingWidget) {
        m_loadingWidget->showLoading();
    }
    QTimer::singleShot(200, this, &DevDiskImageHelper::start);
}

void DevDiskImageHelper::showStatus(const QString &message, bool isError)
{
    if (isError) {
        showRetryUI(message);
    } else {
        if (m_statusLabel) {
            m_statusLabel->setText(message);
        }
    }

    show();
}

/*
    waiting is sometimes required because services
    may not become available
    as soon as the img is mounted
*/
void DevDiskImageHelper::finishWithSuccess(bool wait)
{
    qDebug() << "finishWithSuccess called with wait =" << wait;
    auto handler = [this]() {
        if (m_loadingWidget) {
            m_loadingWidget->stop(false);
        }
        accept();
    };
    if (wait) {
        return QTimer::singleShot(3000, handler);
    }
    handler();
}

void DevDiskImageHelper::finishWithError(const QString &errorMessage)
{
    showRetryUI(errorMessage);
}
