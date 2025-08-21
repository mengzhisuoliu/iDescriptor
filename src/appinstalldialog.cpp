#include "appinstalldialog.h"
#include "appcontext.h"
#include "appdownloadbasedialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

AppInstallDialog::AppInstallDialog(const QString &appName,
                                   const QString &description, QWidget *parent)
    : AppDownloadBaseDialog(appName, parent)
{
    setWindowTitle("Install " + appName);
    setModal(true);
    // setFixedSize(500, 350);
    setFixedWidth(500);

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(this->layout());
    // App info section
    QHBoxLayout *appInfoLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel();
    QPixmap icon = QApplication::style()
                       ->standardIcon(QStyle::SP_ComputerIcon)
                       .pixmap(64, 64);
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(64, 64);
    appInfoLayout->addWidget(iconLabel);

    QVBoxLayout *detailsLayout = new QVBoxLayout();
    QLabel *nameLabel = new QLabel(appName);
    nameLabel->setStyleSheet(
        "font-size: 20px; font-weight: bold; color: #333;");
    detailsLayout->addWidget(nameLabel);

    QLabel *descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 14px; color: #666;");
    detailsLayout->addWidget(descLabel);

    appInfoLayout->addLayout(detailsLayout);
    appInfoLayout->addStretch();
    layout->insertLayout(0, appInfoLayout);

    QLabel *deviceLabel = new QLabel("Choose Device:");
    deviceLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #333;");
    layout->insertWidget(1, deviceLabel);

    m_deviceCombo = new QComboBox();
    m_deviceCombo->setStyleSheet("padding: 8px; border: 1px solid #ddd; "
                                 "border-radius: 4px; font-size: 14px;");
    updateDeviceList();
    layout->insertWidget(2, m_deviceCombo);

    layout->addStretch();

    m_actionButton = new QPushButton("Install");
    m_actionButton->setFixedHeight(40);
    bool hasDevices = m_deviceCombo->count() > 0;
    if (hasDevices) {
        m_actionButton->setStyleSheet(
            "background-color: #34C759; color: white; border: none; "
            "border-radius: 6px; font-size: 16px; font-weight: bold;");
        m_actionButton->setEnabled(true);
    } else {
        m_actionButton->setStyleSheet(
            "background-color: #cccccc; color: #666; border: none; "
            "border-radius: 6px; font-size: 16px; font-weight: bold;");
        m_actionButton->setEnabled(false);
    }
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
}

void AppInstallDialog::updateDeviceList()
{
    m_deviceCombo->clear();
    auto devices = AppContext::sharedInstance()->getAllDevices();
    if (devices.empty()) {
        m_deviceCombo->addItem("No devices connected");
        m_deviceCombo->setEnabled(false);
    } else {
        m_deviceCombo->setEnabled(true);
        for (const auto &device : devices) {
            QString deviceName =
                QString::fromStdString(device->deviceInfo.productType);
            QString deviceId = QString::fromStdString(device->udid);
            m_deviceCombo->addItem(
                deviceName + " (" + deviceId.left(8) + "...)", deviceId);
        }
    }
}

void AppInstallDialog::onInstallClicked()
{
    if (m_deviceCombo->count() == 0) {
        QMessageBox::warning(this, "No Device",
                             "Please connect a device first.");
        return;
    }
    m_deviceCombo->setEnabled(false);
    QString selectedDevice = m_deviceCombo->currentData().toString();
    // QStringList args = {"download",
    //                     "-b",
    //                     m_bundleId,
    //                     "-o",
    //                     "./",
    //                     "--purchase",
    //                     "--keychain-passphrase",
    //                     "iDescriptor",
    //                     "--format",
    //                     "json"};
    m_actionButton->setEnabled(false);

    int buttonIndex = m_layout->indexOf(m_actionButton);
    layout()->removeWidget(m_actionButton);
    m_actionButton->deleteLater();
    m_actionButton = nullptr; // Reset to avoid double deletion
    // TODO: need to implement the actual installation logic
    // startDownloadProcess(args, QDir::currentPath(), buttonIndex);
}
