#include "appinstalldialog.h"
#include "appcontext.h"
#include "appdownloadbasedialog.h"
#include "iDescriptor.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFutureWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

AppInstallDialog::AppInstallDialog(const QString &appName,
                                   const QString &description,
                                   const QString &bundleId, QWidget *parent)
    : AppDownloadBaseDialog(appName, bundleId, parent), m_bundleId(bundleId),
      m_statusLabel(nullptr), m_installWatcher(nullptr)
{
    setWindowTitle("Install " + appName + " - iDescriptor");
    setModal(true);
    setFixedWidth(500);

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(this->layout());
    // App info section
    QHBoxLayout *appInfoLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel();
    fetchAppIconFromApple(
        bundleId,
        [iconLabel](const QPixmap &pixmap) {
            if (!pixmap.isNull()) {
                QPixmap scaled =
                    pixmap.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                  Qt::SmoothTransformation);
                QPixmap rounded(64, 64);
                rounded.fill(Qt::transparent);

                QPainter painter(&rounded);
                painter.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addRoundedRect(QRectF(0, 0, 64, 64), 16, 16);
                painter.setClipPath(path);
                painter.drawPixmap(0, 0, scaled);
                painter.end();

                iconLabel->setPixmap(rounded);
            }
        },
        this);
    QPixmap icon = QApplication::style()
                       ->standardIcon(QStyle::SP_ComputerIcon)
                       .pixmap(64, 64);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(64, 64);
    appInfoLayout->addWidget(iconLabel);

    QVBoxLayout *detailsLayout = new QVBoxLayout();
    QLabel *nameLabel = new QLabel(appName);
    nameLabel->setStyleSheet("font-size: 20px; font-weight: bold;");
    detailsLayout->addWidget(nameLabel);

    QLabel *descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 14px;");
    detailsLayout->addWidget(descLabel);

    appInfoLayout->addLayout(detailsLayout);
    appInfoLayout->addStretch();
    layout->insertLayout(0, appInfoLayout);

    QLabel *deviceLabel = new QLabel("Choose Device:");
    deviceLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    layout->insertWidget(1, deviceLabel);

    m_deviceCombo = new QComboBox();
    layout->insertWidget(2, m_deviceCombo);

    m_statusLabel = new QLabel("Ready to install");
    m_statusLabel->setStyleSheet("font-size: 14px; padding: 5px;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->insertWidget(3, m_statusLabel);

    layout->addStretch();

    m_actionButton = new QPushButton("Install");
    m_actionButton->setFixedHeight(40);

    connect(m_actionButton, &QPushButton::clicked, this,
            &AppInstallDialog::onInstallClicked);
    layout->addWidget(m_actionButton);

    QPushButton *cancelButton = new QPushButton("Cancel");
    cancelButton->setFixedHeight(40);
    cancelButton->setStyleSheet(
        "background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "border-radius: 6px; font-size: 16px;");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(cancelButton);

    connect(AppContext::sharedInstance(), &AppContext::deviceChange, this,
            &AppInstallDialog::updateDeviceList);

    updateDeviceList();
}

void AppInstallDialog::updateDeviceList()
{
    m_deviceCombo->clear();
    auto devices = AppContext::sharedInstance()->getAllDevices();
    if (devices.empty()) {
        m_deviceCombo->addItem("No devices connected");
        m_deviceCombo->setEnabled(false);
        m_actionButton->setDefault(false);
        m_actionButton->setEnabled(false);
        m_statusLabel->setText("No devices connected");
    } else {
        m_deviceCombo->setEnabled(true);
        for (const auto &device : devices) {
            QString deviceName =
                QString::fromStdString(device->deviceInfo.productType);
            QString deviceId = QString::fromStdString(device->udid);
            m_deviceCombo->addItem(
                deviceName + " / " + deviceId.left(8) + "...", deviceId);
        }
        m_actionButton->setDefault(true);
        m_actionButton->setEnabled(true);
        m_statusLabel->setText("Ready to install");
    }
}

void AppInstallDialog::performInstallation(const QString &ipaPath,
                                           const QString &deviceUdid)
{
    m_statusLabel->setText("Installing app...");

    // Setup install watcher
    m_installWatcher = new QFutureWatcher<int>(this);
    connect(m_installWatcher, &QFutureWatcher<int>::finished, this, [this]() {
        int result = m_installWatcher->result();
        m_installWatcher->deleteLater();
        m_installWatcher = nullptr;

        if (result == 0) {
            m_statusLabel->setText("Installation completed successfully!");
            m_statusLabel->setStyleSheet(
                "font-size: 14px; color: #34C759; padding: 5px;");
            QMessageBox::information(this, "Success",
                                     "App installed successfully!");
            accept();
        } else {
            m_statusLabel->setText("Installation failed");
            m_statusLabel->setStyleSheet(
                "font-size: 14px; color: #FF3B30; padding: 5px;");
            QMessageBox::critical(
                this, "Error",
                QString("Installation failed with error code: %1").arg(result));
        }
    });

    // Run installation in background thread
    QFuture<int> future = QtConcurrent::run([ipaPath, deviceUdid]() -> int {
        iDescriptorDevice *device =
            AppContext::sharedInstance()->getDevice(deviceUdid.toStdString());
        if (!device) {
            return -1;
        }

        instproxy_error_t ret = install_IPA(device->device, device->afcClient,
                                            ipaPath.toStdString().c_str());
        return static_cast<int>(ret);
    });

    m_installWatcher->setFuture(future);
}
void AppInstallDialog::onInstallClicked()
{
    if (m_deviceCombo->count() == 0) {
        QMessageBox::warning(this, "No Device",
                             "Please connect a device first.");
        return;
    }

    m_deviceCombo->setEnabled(false);
    m_actionButton->setEnabled(false);
    m_statusLabel->setText("Downloading app...");

    QString selectedDevice = m_deviceCombo->currentData().toString();

    int buttonIndex = m_layout->indexOf(m_actionButton);
    layout()->removeWidget(m_actionButton);
    m_actionButton->deleteLater();
    m_actionButton = nullptr;

    if (m_tempDir) {
        delete m_tempDir;
        m_tempDir = nullptr;
    }
    // Create a new temporary directory for each installation
    m_tempDir = new QTemporaryDir();
    if (!m_tempDir->isValid()) {
        m_statusLabel->setText("Failed to create temporary directory");
        m_statusLabel->setStyleSheet(
            "font-size: 14px; color: #FF3B30; padding: 5px;");
        QMessageBox::critical(
            this, "Error",
            "Could not create temporary directory for download.");
        return;
    }

    startDownloadProcess(m_bundleId, m_tempDir->path(), buttonIndex, false);
    connect(this, &AppDownloadBaseDialog::downloadFinished, this,
            [this, selectedDevice](bool success) {
                if (success) {
                    qDebug() << "Download finished, starting installation...";
                    /*
                        FIXME: libipatool generates random id and appends that
                       to the downloaded IPA filename, so we need to search for
                       it.
                    */
                    // Find the actual downloaded IPA file
                    QDir outDir(m_tempDir->path());
                    QStringList filters;
                    filters << m_bundleId + "*.ipa";
                    QStringList matches =
                        outDir.entryList(filters, QDir::Files, QDir::Time);
                    if (matches.isEmpty()) {
                        m_statusLabel->setText(
                            "Download failed - IPA not found");
                        m_statusLabel->setStyleSheet(
                            "font-size: 14px; color: #FF3B30; padding: 5px;");
                        QMessageBox::critical(
                            this, "Error",
                            QString("Downloaded IPA not found in %1")
                                .arg(outDir.absolutePath()));
                        return;
                    }

                    QString ipaFile = outDir.filePath(matches.first());
                    performInstallation(ipaFile, selectedDevice);

                } else {
                    m_statusLabel->setText("Download failed");
                    m_statusLabel->setStyleSheet(
                        "font-size: 14px; color: #FF3B30; padding: 5px;");
                }
            });
}

void AppInstallDialog::reject()
{
    // Cancel installation if it's running
    if (m_installWatcher && !m_installWatcher->isFinished()) {
        m_installWatcher->cancel();
        m_installWatcher->deleteLater();
        m_installWatcher = nullptr;
        if (m_statusLabel) {
            m_statusLabel->setText("Installation cancelled");
            m_statusLabel->setStyleSheet(
                "font-size: 14px; color: #FF3B30; padding: 5px;");
        }
    }

    AppDownloadBaseDialog::reject();
}

AppInstallDialog::~AppInstallDialog()
{
    if (m_tempDir) {
        delete m_tempDir;
        m_tempDir = nullptr;
    }
}