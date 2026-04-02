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

#include "cableinfowidget.h"
#include "appcontext.h"
#include <QApplication>
#include <QDebug>
#include <QGroupBox>
#include <QScrollArea>
#include <QTimer>

#ifdef WIN32
#include "platform/windows/win_common.h"
#endif

CableInfoWidget::CableInfoWidget(
    const std::shared_ptr<iDescriptorDevice> device, QWidget *parent)
    : Tool(parent), m_device(device)
{
    setupUI();
    setAttribute(Qt::WA_DeleteOnClose);
    resize(600, 400);

    connect(m_device->service_manager,
            &CXX::ServiceManager::cable_info_retrieved, this,
            [this](const QString &response) {
                m_response = response;
                analyzeCableInfo();
                updateUI();
            });

    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this](const QString &udid) {
                if (m_device->udid == udid) {
                    this->close();
                }
            });
    QTimer::singleShot(200, this, &CableInfoWidget::initCableInfo);
}

void CableInfoWidget::setupUI()
{
    setWindowTitle("Cable Information - iDescriptor");

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *contentContainer = new QWidget(this);
    m_mainLayout = new QVBoxLayout(contentContainer);
    m_mainLayout->setSpacing(20);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header section
    QHBoxLayout *headerLayout = new QHBoxLayout();

    m_statusLabel = new QLabel("Analyzing cable...");
    m_statusLabel->setStyleSheet("QLabel { "
                                 "font-size: 18px; "
                                 "}");
    m_descriptionLabel =
        new QLabel("Please wait while we analyze the connected cable.");
    m_descriptionLabel->setStyleSheet("font-size: 9px;");

    QPushButton *redoButton = new QPushButton("Re-analyze");
    connect(redoButton, &QPushButton::clicked, this, [this]() {
        m_loadingWidget->showLoading();
        QTimer::singleShot(200, this, &CableInfoWidget::initCableInfo);
    });
    headerLayout->addWidget(m_statusLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(redoButton);

    m_mainLayout->addLayout(headerLayout);

    m_infoWidget = new QGroupBox("Cable Information");

    m_infoLayout = new QGridLayout(m_infoWidget);
    m_infoLayout->setSpacing(12);
    m_infoLayout->setColumnStretch(1, 1);

    m_mainLayout->addWidget(m_descriptionLabel);
    m_mainLayout->addWidget(m_infoWidget);
    m_mainLayout->addStretch();

    m_loadingWidget = new ZLoadingWidget(true, this);
    rootLayout->addWidget(m_loadingWidget);
    m_loadingWidget->setupContentWidget(contentContainer);

    connect(m_loadingWidget, &ZLoadingWidget::retryClicked, this, [this]() {
        m_loadingWidget->showLoading();
        QTimer::singleShot(200, this, &CableInfoWidget::initCableInfo);
    });
}

void CableInfoWidget::initCableInfo()
{
    if (!m_device) {
        m_loadingWidget->showError("Something went wrong (no device ?)");
        return;
    }

    m_statusLabel->setText("Analyzing cable...");
    m_device->service_manager->get_cable_info();
}

void CableInfoWidget::analyzeCableInfo()
{
    // FIXME: genuine check is not perfect, still need more research
    // The 'return;' statement here prevents the entire function from executing.
    // It should be removed for the parsing logic to run.
    // return;

    qDebug() << "Analyzing cable info...";

    m_cableInfo = CableInfo();

    // Original logic: `if (!m_response.isEmpty()) { ... showError ... return;
    // }` This meant if the response was NOT empty, it showed an error and
    // returned. The correct logic is to show an error if the response IS empty.
    if (m_response.isEmpty()) {
        m_loadingWidget->showError("No cable information retrieved.");
        return;
    }

    pugi::xml_document doc;
    auto res = doc.load_string(m_response.toUtf8().constData());
    if (!res) {
        m_loadingWidget->showError("Failed to parse cable information.");
        return;
    }
    XmlPlistDict ioreg(doc.child("plist").child("dict"));

    if (!ioreg.valid()) {
        m_loadingWidget->showError(
            "Failed to find plist dictionary in response.");
        return;
    }
    m_cableInfo.isConnected = ioreg["ConnectionActive"].getBool();

    // Check if genuine (Apple manufacturer and valid model info)
    m_cableInfo.manufacturer = QString::fromStdString(
        ioreg["IOAccessoryAccessoryManufacturer"].getString());
    m_cableInfo.modelNumber = QString::fromStdString(
        ioreg["IOAccessoryAccessoryModelNumber"].getString());
    m_cableInfo.accessoryName =
        QString::fromStdString(ioreg["IOAccessoryAccessoryName"].getString());
    m_cableInfo.serialNumber = QString::fromStdString(
        ioreg["IOAccessoryAccessorySerialNumber"].getString());
    m_cableInfo.interfaceModuleSerial = QString::fromStdString(
        ioreg["IOAccessoryInterfaceModuleSerialNumber"].getString());

    // Check if Type-C (based on accessory name or TriStar class)
    m_cableInfo.triStarClass =
        QString::fromStdString(ioreg["TriStarICClass"].getString());
    m_cableInfo.isTypeC =
        (m_cableInfo.accessoryName.contains("USB-C", Qt::CaseInsensitive) ||
         m_cableInfo.triStarClass.contains("1612")); // CBTL1612 is Type-C

    // Determine if genuine based on manufacturer and presence of detailed info
    bool preGenuineCheck =
        (m_cableInfo.manufacturer.contains("Apple", Qt::CaseInsensitive) &&
         !m_cableInfo.modelNumber.isEmpty() &&
         !m_cableInfo.accessoryName.isEmpty());

    // Further checks for Type-C cables
    // if report says it's Type-C, it must match the actual connection type
    if (m_cableInfo.isTypeC) {
        bool actuallyTypeC =
            m_device->deviceInfo.batteryInfo.usbConnectionType ==
            BatteryInfo::ConnectionType::USB_TYPEC;
        if (!actuallyTypeC) {
            // most likely a fake cable with faked info
            m_cableInfo.isFakeInfo = true;
        }
        m_cableInfo.isGenuine = actuallyTypeC && preGenuineCheck;
    } else {
        m_cableInfo.isGenuine = preGenuineCheck;
    }

    // Power information
    m_cableInfo.currentLimit = ioreg["IOAccessoryUSBCurrentLimit"].getUInt();
    m_cableInfo.chargingVoltage =
        ioreg["IOAccessoryUSBChargingVoltage"].getUInt();

    // Connection type
    QString connectString = QString::fromStdString(
        ioreg["IOAccessoryUSBConnectString"].getString());
    int connectType =
        static_cast<int>(ioreg["IOAccessoryUSBConnectType"].getUInt());
    m_cableInfo.connectionType =
        QString("%1 (Type %2)").arg(connectString).arg(connectType);

    // Supported and active transports
    // In XML plists, an array is typically represented by an <array> tag,
    // and its elements by <string>, <integer>, etc. tags.

    // Handle "TransportsSupported"
    XmlPlistDict supportedTransportsWrapper = ioreg["TransportsSupported"];
    // Assuming XmlPlistDict has a 'node()' method to access its underlying
    // pugi::xml_node
    pugi::xml_node supportedTransportsNode =
        supportedTransportsWrapper.getNode();

    if (supportedTransportsNode &&
        supportedTransportsNode.name() ==
            "array") { // Check if it's a valid node and if its tag name is
                       // "array"
        for (pugi::xml_node transportChild :
             supportedTransportsNode.children()) {
            if (transportChild.name() ==
                "string") { // Assume each item in the array is a <string> node
                m_cableInfo.supportedTransports.append(
                    QString::fromStdString(transportChild.text().as_string()));
            }
        }
    }

    // Handle "TransportsActive" similarly
    XmlPlistDict activeTransportsWrapper = ioreg["TransportsActive"];
    pugi::xml_node activeTransportsNode = activeTransportsWrapper.getNode();

    if (activeTransportsNode && activeTransportsNode.name() == "array") {
        for (pugi::xml_node transportChild : activeTransportsNode.children()) {
            if (transportChild.name() == "string") {
                m_cableInfo.activeTransports.append(
                    QString::fromStdString(transportChild.text().as_string()));
            }
        }
    }
}

void CableInfoWidget::updateUI()
{
    // Clear existing info
    QLayoutItem *item;
    while ((item = m_infoLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (!m_cableInfo.isConnected) {
        m_loadingWidget->showError(
            QString("%1 does not seem to be connected to any cable.")
                .arg(QString::fromStdString(m_device->deviceInfo.productType)));
        return;
    }

    // Update status and icon based on cable type
    QString statusText;
    QString statusStyle;

    m_descriptionLabel->setText("Please note that this check may not be "
                                "absolute guarantee of authenticity.");
    if (m_cableInfo.isGenuine) {
        // todo: type-c to type-c
        statusText = QString("Genuine %1")
                         .arg(m_cableInfo.isTypeC ? "USB-C to Lightning Cable"
                                                  : "Lightning Cable");
        statusStyle =
            "QLabel { color: #28a745; font-size: 18px; font-weight: bold; }";
    } else {
        statusText = "Third-party Cable";
        statusStyle =
            "QLabel { color: #dc3545; font-size: 18px; font-weight: bold; }";

        if (m_cableInfo.isFakeInfo) {
            m_descriptionLabel->setText("The cable reports false information. "
                                        "It is most likely a fake cable.");
        }
    }

    m_statusLabel->setText(statusText);
    m_statusLabel->setStyleSheet(statusStyle);

    int row = 0;

    // Basic information
    if (!m_cableInfo.accessoryName.isEmpty()) {
        createInfoRow(m_infoLayout, row++, "Name:", m_cableInfo.accessoryName);
    }

    if (!m_cableInfo.manufacturer.isEmpty()) {
        createInfoRow(m_infoLayout, row++,
                      "Manufacturer:", m_cableInfo.manufacturer);
    }

    if (!m_cableInfo.modelNumber.isEmpty()) {
        createInfoRow(m_infoLayout, row++, "Model:", m_cableInfo.modelNumber);
    }

    if (!m_cableInfo.serialNumber.isEmpty()) {
        createInfoRow(m_infoLayout, row++,
                      "Serial Number:", m_cableInfo.serialNumber);
    }

    if (!m_cableInfo.interfaceModuleSerial.isEmpty()) {
        createInfoRow(m_infoLayout, row++,
                      "Interface Module:", m_cableInfo.interfaceModuleSerial);
    }

    // Technical information
    createInfoRow(m_infoLayout, row++, "Cable Type:",
                  m_cableInfo.isTypeC ? "USB-C to Lightning"
                                      : "Lightning to USB-A");

    if (m_cableInfo.currentLimit > 0) {
        createInfoRow(m_infoLayout, row++, "Current Limit:",
                      QString("%1 mA").arg(m_cableInfo.currentLimit));
    }

    if (m_cableInfo.chargingVoltage > 0) {
        createInfoRow(m_infoLayout, row++, "Charging Voltage:",
                      QString("%1 mV").arg(m_cableInfo.chargingVoltage));
    }

    if (!m_cableInfo.connectionType.isEmpty()) {
        createInfoRow(m_infoLayout, row++,
                      "Connection:", m_cableInfo.connectionType);
    }

    if (!m_cableInfo.triStarClass.isEmpty()) {
        createInfoRow(m_infoLayout, row++,
                      "Controller:", m_cableInfo.triStarClass);
    }

    // Transport information
    if (!m_cableInfo.activeTransports.isEmpty()) {
        createInfoRow(m_infoLayout, row++, "Active Transports:",
                      m_cableInfo.activeTransports.join(", "));
    }

    if (!m_cableInfo.supportedTransports.isEmpty()) {
        createInfoRow(m_infoLayout, row++, "Supported Transports:",
                      m_cableInfo.supportedTransports.join(", "));
    }
    m_loadingWidget->stop(true);
}

void CableInfoWidget::createInfoRow(QGridLayout *layout, int row,
                                    const QString &label, const QString &value)
{
    QLabel *labelWidget = new QLabel(label);

    QLabel *valueWidget = new QLabel(value);
    valueWidget->setWordWrap(true);

    layout->addWidget(labelWidget, row, 0, Qt::AlignTop);
    layout->addWidget(valueWidget, row, 1, Qt::AlignTop);
}