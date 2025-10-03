#include "deviceinfowidget.h"
#include "batterywidget.h"
#include "diskusagewidget.h"
#include "fileexplorerwidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QApplication>
#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPainter>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

DeviceInfoWidget::DeviceInfoWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_device(device)
{
    // Main layout with horizontal orientation
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);
    m_graphicsScene = new QGraphicsScene(this); // no parent
    QGraphicsPixmapItem *pixmapItem =
        new QGraphicsPixmapItem(QPixmap(":/resources/iphone.png"));
    m_graphicsScene->addItem(pixmapItem);

    m_graphicsView = new ResponsiveGraphicsView(m_graphicsScene, this);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setMinimumWidth(200);
    m_graphicsView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_graphicsView->setStyleSheet("background: transparent; border: none;");

    mainLayout->addWidget(m_graphicsView, 1); // Stretch factor 1

    // Right side: Info Table
    QWidget *infoContainer = new QWidget();
    // infoContainer->setObjectName("infoContainer");
    // infoContainer->setStyleSheet("QWidget#infoContainer { "
    //                              "   border: 1px solid #ccc; "
    //                              "   border-radius: 6px; "
    //                              "}");
    infoContainer->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    QVBoxLayout *infoLayout = new QVBoxLayout(infoContainer);
    // infoLayout->setSpacing(10);

    // Header
    QGroupBox *headerWidget = new QGroupBox();
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setSpacing(15);

    QLabel *devProductType =
        new QLabel(QString::fromStdString(device->deviceInfo.productType));
    devProductType->setStyleSheet("font-size: 1rem; font-weight: bold;");

    QLabel *diskCapacityLabel = new QLabel(
        QString::number(device->deviceInfo.diskInfo.totalDiskCapacity /
                        (1000 * 1000 * 1000)) +
        " GB");

    diskCapacityLabel->setSizePolicy(QSizePolicy::Maximum,
                                     QSizePolicy::Preferred);
    diskCapacityLabel->setAttribute(Qt::WA_StyledBackground, true);
    diskCapacityLabel->setStyleSheet("background-color: rgba(0, 255, 30, 0.5);"
                                     "padding: 4px;"
                                     "border-radius: 13px;");

    m_chargingStatusLabel =
        new QLabel(device->deviceInfo.batteryInfo.isCharging ? "Charging"
                                                             : "Not Charging");
    m_chargingStatusLabel->setStyleSheet(
        device->deviceInfo.batteryInfo.isCharging
            ? QString("color: %1;").arg(COLOR_GREEN.name())
            : "color: white;");

    // Create the layout without a parent widget
    QHBoxLayout *chargingLayout = new QHBoxLayout();
    chargingLayout->setContentsMargins(0, 0, 0, 0);
    chargingLayout->setSpacing(5);

    // Create icon label
    m_lightningIconLabel = new QLabel();
    QPixmap lightningIcon(":/icons/MdiLightningBolt.png");
    QPixmap scaledIcon = lightningIcon.scaled(26, 26, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation);
    m_lightningIconLabel->setPixmap(scaledIcon);
    m_batteryWidget =
        new BatteryWidget(device->deviceInfo.batteryInfo.currentBatteryLevel,
                          device->deviceInfo.batteryInfo.isCharging, this);

    // Add the widgets to the new layout
    chargingLayout->addWidget(m_chargingStatusLabel);
    chargingLayout->addWidget(m_lightningIconLabel);
    chargingLayout->addWidget(m_batteryWidget);

    m_chargingWattsWithCableTypeLabel = new QLabel(
        QString::number(device->deviceInfo.batteryInfo.watts) + "W" + "/" +
        (device->deviceInfo.batteryInfo.usbConnectionType ==
                 BatteryInfo::ConnectionType::USB
             ? "USB"
             : "USB-C"));

    headerLayout->addWidget(devProductType);
    headerLayout->addWidget(diskCapacityLabel);
    headerLayout->addStretch(); // Push items to the left
    headerLayout->addLayout(chargingLayout);
    headerLayout->addWidget(m_chargingWattsWithCableTypeLabel);

    infoLayout->addWidget(headerWidget);
    // add spacer
    infoLayout->addSpacerItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    // Add maximum stretch between header and grid
    infoLayout->addStretch();

    QGroupBox *gridContainer = new QGroupBox("Device Information");
    QGridLayout *gridLayout = new QGridLayout(); // Set layout on gridWidget
    gridLayout->setSpacing(8);
    gridLayout->setColumnStretch(1, 1); // Allow value column to stretch
    gridLayout->setColumnStretch(
        3, 1); // Allow value column for right side to stretch
    gridLayout->setContentsMargins(17, 17, 17, 17);
    gridContainer->setLayout(gridLayout);
    QList<QPair<QString, QWidget *>> infoItems;

    auto createValueLabel = [](const QString &text) {
        return new QLabel(text);
    };

    infoItems.append({"iOS Version:", createValueLabel(QString::fromStdString(
                                          device->deviceInfo.productVersion))});
    infoItems.append({"Device Name:", createValueLabel(QString::fromStdString(
                                          device->deviceInfo.deviceName))});

    // Activation state label with color and tooltip
    QLabel *activationLabel = new QLabel;
    QString stateText;
    QString tooltipText;
    QColor color;

    switch (device->deviceInfo.activationState) {
    case DeviceInfo::ActivationState::Activated:
        stateText = "Activated";
        color = COLOR_GREEN;
        tooltipText = "Device is activated and ready for use.";
        break;
    case DeviceInfo::ActivationState::FactoryActivated:
        stateText = "Factory Activated";
        color = COLOR_ORANGE;
        tooltipText = "Activation is most likely bypassed.";
        break;
    default:
        stateText = "Unactivated";
        color = COLOR_RED;
        tooltipText = "Device is not activated and requires setup.";
        break;
    }

    activationLabel->setText(stateText);
    activationLabel->setStyleSheet("color: " + color.name() + ";");
    activationLabel->setToolTip(tooltipText);
    infoItems.append({"Activation State:", activationLabel});

    infoItems.append({"Device Class:", createValueLabel(QString::fromStdString(
                                           device->deviceInfo.deviceClass))});
    infoItems.append({"Device Color:", createValueLabel(QString::fromStdString(
                                           device->deviceInfo.deviceColor))});
    infoItems.append(
        {"Jailbroken:", createValueLabel(QString::fromStdString(
                            device->deviceInfo.jailbroken ? "Yes" : "No"))});
    infoItems.append({"Model Number:", createValueLabel(QString::fromStdString(
                                           device->deviceInfo.modelNumber))});
    infoItems.append(
        {"CPU Architecture:", createValueLabel(QString::fromStdString(
                                  device->deviceInfo.cpuArchitecture))});
    infoItems.append({"Build Version:", createValueLabel(QString::fromStdString(
                                            device->deviceInfo.buildVersion))});
    infoItems.append(
        {"Hardware Model:", createValueLabel(QString::fromStdString(
                                device->deviceInfo.hardwareModel))});
    infoItems.append(
        {"Hardware Platform:", createValueLabel(QString::fromStdString(
                                   device->deviceInfo.hardwarePlatform))});
    infoItems.append(
        {"Ethernet Address:", createValueLabel(QString::fromStdString(
                                  device->deviceInfo.ethernetAddress))});
    infoItems.append(
        {"Bluetooth Address:", createValueLabel(QString::fromStdString(
                                   device->deviceInfo.bluetoothAddress))});
    infoItems.append(
        {"Firmware Version:", createValueLabel(QString::fromStdString(
                                  device->deviceInfo.firmwareVersion))});

    // Battery Info
    QWidget *batteryWidget = new QWidget();
    QHBoxLayout *batteryLayout = new QHBoxLayout(batteryWidget);
    batteryLayout->setContentsMargins(0, 0, 0, 0);
    batteryLayout->setSpacing(5);
    batteryLayout->addWidget(new QLabel(device->deviceInfo.batteryInfo.health));
    QPushButton *moreButton = new QPushButton("More");
    connect(moreButton, &QPushButton::clicked, this,
            &DeviceInfoWidget::onBatteryMoreClicked);
    batteryLayout->addWidget(moreButton);
    batteryLayout->addStretch();
    infoItems.append({"Battery Health:", batteryWidget});

    infoItems.append(
        {"Production Device:",
         createValueLabel(QString::fromStdString(
             device->deviceInfo.productionDevice ? "Yes" : "No"))});

    // Distribute items into the grid
    int numRows = (infoItems.size() + 1) / 2;
    for (int i = 0; i < numRows; ++i) {
        // Left column item
        QLabel *keyLabelLeft = new QLabel(infoItems[i].first);
        keyLabelLeft->setStyleSheet("font-weight: bold;");
        gridLayout->addWidget(keyLabelLeft, i, 0);
        gridLayout->addWidget(infoItems[i].second, i, 1);

        // Right column item
        int rightIndex = i + numRows;
        if (rightIndex < infoItems.size()) {
            QLabel *keyLabelRight = new QLabel(infoItems[rightIndex].first);
            keyLabelRight->setStyleSheet("font-weight: bold;");
            gridLayout->addWidget(keyLabelRight, i, 2);
            gridLayout->addWidget(infoItems[rightIndex].second, i, 3);
        }
    }

    infoLayout->addWidget(gridContainer);
    // infoLayout->addStretch(); // Pushes footer to the bottom

    // Footer
    QLabel *footerLabel =
        new QLabel("UDID: " + QString::fromStdString(device->udid));
    footerLabel->setStyleSheet(
        "font-size: 10px; color: #666; padding-top: 5px; "
        "border-top: 1px solid #eee;");
    footerLabel->setWordWrap(true);
    infoLayout->addWidget(footerLabel);

    // Create a vertical layout for the right side to stack info and disk usage
    QVBoxLayout *rightSideLayout = new QVBoxLayout();
    rightSideLayout->setSpacing(10);
    rightSideLayout->addWidget(infoContainer);
    rightSideLayout->addWidget(new DiskUsageWidget(device, this));
    rightSideLayout->setAlignment(Qt::AlignCenter);
    mainLayout->addLayout(rightSideLayout, 2); // Stretch factor 2

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this,
            &DeviceInfoWidget::updateBatteryInfo);
    m_updateTimer->start(30000); // Update every 30 seconds
}

DeviceInfoWidget::~DeviceInfoWidget()
{
    if (m_graphicsView) {
        m_graphicsView->setScene(
            nullptr); // prevents QGraphicsScene from calling into view during
                      // its destructor only needed on macos ?
    }
}

void DeviceInfoWidget::onBatteryMoreClicked()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Battery Details");
    QString details =
        "Battery Cycle Count: " +
        QString::number(m_device->deviceInfo.batteryInfo.cycleCount) + "\n" +
        "Battery Serial Number: " +
        QString::fromStdString(m_device->deviceInfo.batteryInfo.serialNumber);
    msgBox.setText(details);
    msgBox.exec();
}

QPixmap DeviceInfoWidget::getDeviceIcon(const std::string &productType)
{
    // Create a simple colored icon based on device type
    QPixmap icon(16, 16);
    icon.fill(Qt::transparent);

    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);

    if (productType.find("iPhone") != std::string::npos) {
        painter.setBrush(QColor(0, 122, 255)); // iOS blue
        painter.drawEllipse(2, 2, 12, 12);
    } else if (productType.find("iPad") != std::string::npos) {
        painter.setBrush(QColor(255, 149, 0)); // Orange
        painter.drawRect(2, 2, 12, 12);
    } else {
        painter.setBrush(QColor(128, 128, 128)); // Gray for unknown
        painter.drawEllipse(2, 2, 12, 12);
    }

    return icon;
}

void DeviceInfoWidget::updateBatteryInfo()
{
    qDebug() << "Updating battery info...";
    plist_t diagnostics = nullptr;
    get_battery_info(m_device->deviceInfo.rawProductType, m_device->device,
                     m_device->deviceInfo.is_iPhone, diagnostics);

    if (!diagnostics) {
        qDebug() << "Failed to get diagnostics plist.";
        return;
    }
    /*DATA*/
    DeviceInfo &d = m_device->deviceInfo;
    qDebug() << "old device" << d.oldDevice;
    PlistNavigator ioreg = PlistNavigator(diagnostics)["IORegistry"];
    if (d.oldDevice)
        parseOldDeviceBattery(ioreg, d);
    else
        parseDeviceBattery(ioreg, d);
    /*UI*/
    updateChargingStatusIcon();
    m_chargingWattsWithCableTypeLabel->setText(
        QString::number(d.batteryInfo.watts) + "W" + "/" +
        (d.batteryInfo.usbConnectionType == BatteryInfo::ConnectionType::USB
             ? "USB"
             : "USB-C"));

    m_batteryWidget->updateContext(d.batteryInfo.isCharging,
                                   d.batteryInfo.currentBatteryLevel);
}

void DeviceInfoWidget::updateChargingStatusIcon()
{
    if (m_device->deviceInfo.batteryInfo.isCharging) {
        m_chargingStatusLabel->setText("Charging");
        m_chargingStatusLabel->setStyleSheet(
            QString("color: %1;").arg(COLOR_GREEN.name()));
        m_lightningIconLabel->show();

    } else {
        m_chargingStatusLabel->setText("Not Charging");
        m_chargingStatusLabel->setStyleSheet("");
        m_lightningIconLabel->hide();
    }
}