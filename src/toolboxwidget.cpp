#include "toolboxwidget.h"
#include "airplaywindow.h"
#include "appcontext.h"
#include "cableinfowidget.h"
#include "devdiskimageswidget.h"
#include "devdiskmanager.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#ifndef __APPLE__
#include "ifusewidget.h"
#endif
#include "querymobilegestaltwidget.h"
#include "realtimescreenwidget.h"
#include "virtuallocationwidget.h"
#include "wirelessphotoimportwidget.h"
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QStyle>

struct iDescriptorToolWidget {
    iDescriptorTool tool;
    QString description;
    bool requiresDevice;
    QString iconName;
};

bool enterRecoveryMode(iDescriptorDevice *device)
{
    lockdownd_client_t client = NULL;
    lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
    idevice_error_t ret = IDEVICE_E_UNKNOWN_ERROR;

    if (LOCKDOWN_E_SUCCESS !=
        (ldret = lockdownd_client_new(device->device, &client, APP_LABEL))) {
        printf("ERROR: Could not connect to lockdownd: %s (%d)\n",
               lockdownd_strerror(ldret), ldret);
        return false;
    }

    ldret = lockdownd_enter_recovery(client);
    if (ldret == LOCKDOWN_E_SESSION_INACTIVE) {
        lockdownd_client_free(client);
        client = NULL;
        if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(
                                       device->device, &client, APP_LABEL))) {
            printf("ERROR: Could not connect to lockdownd: %s (%d)\n",
                   lockdownd_strerror(ldret), ldret);
            return false;
        }
        ldret = lockdownd_enter_recovery(client);
    }
    lockdownd_client_free(client);
    if (ldret != LOCKDOWN_E_SUCCESS) {
        printf("Failed to enter recovery mode.\n");
        return false;
    } else {
        printf("Device is successfully switching to recovery mode.\n");
        return true;
    }
}

ToolboxWidget::ToolboxWidget(QWidget *parent) : QWidget{parent}
{
    setupUI();
    updateDeviceList();
    updateToolboxStates();

    connect(AppContext::sharedInstance(), &AppContext::deviceChange, this,
            &ToolboxWidget::updateUI);
    connect(AppContext::sharedInstance(),
            &AppContext::currentDeviceSelectionChanged, this,
            &ToolboxWidget::onCurrentDeviceChanged);
}

void ToolboxWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Device selection section
    QHBoxLayout *deviceLayout = new QHBoxLayout();
    m_deviceLabel = new QLabel("Device:");
    m_deviceCombo = new QComboBox();
    m_deviceCombo->setMinimumWidth(200);

    deviceLayout->addWidget(m_deviceLabel);
    deviceLayout->addWidget(m_deviceCombo);
    deviceLayout->setContentsMargins(15, 5, 15, 5);
    deviceLayout->addStretch();

    mainLayout->addLayout(deviceLayout);

    // Scroll area for toolboxes
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }");
    m_scrollArea->viewport()->setStyleSheet("background: transparent;");

    m_contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setSpacing(20);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // Main Tools Section
    QLabel *mainToolsLabel = new QLabel("Tools");
    mainToolsLabel->setStyleSheet(
        "font-weight: bold; font-size: 14px; margin-left: 10px");
    contentLayout->addWidget(mainToolsLabel);

    QWidget *mainToolsWidget = new QWidget();
    m_gridLayout = new QGridLayout(mainToolsWidget);
    m_gridLayout->setSpacing(10);

    QList<iDescriptorToolWidget> mainToolWidgets;
    mainToolWidgets.append(
        {iDescriptorTool::Airplayer, "Cast your device screen ", false, ""});
    mainToolWidgets.append({iDescriptorTool::VirtualLocation,
                            "Simulate GPS location on your device", true, ""});
    mainToolWidgets.append(
        {iDescriptorTool::RealtimeScreen,
         "View device screen in real-time (wired connection required)", true,
         ""});
    mainToolWidgets.append({iDescriptorTool::QueryMobileGestalt,
                            "Query device hardware information", true, ""});
    mainToolWidgets.append({iDescriptorTool::DeveloperDiskImages,
                            "Manage developer disk images", false, ""});
    mainToolWidgets.append(
        {iDescriptorTool::WirelessPhotoImport,
         "Import photos wirelessly to your iDevice (requires Shortcut app)",
         false, ""});
#ifndef __APPLE__
    mainToolWidgets.append({iDescriptorTool::iFuse,
                            "Mount your iPhone's filesystem on your PC", true,
                            ""});
#endif
    mainToolWidgets.append({iDescriptorTool::CableInfoWidget,
                            "View detailed cable and connection info", true,
                            ""});
    mainToolWidgets.append({iDescriptorTool::NetworkDevices,
                            "Discover and monitor devices on your network",
                            false, ""});

    for (int i = 0; i < mainToolWidgets.size(); ++i) {
        const auto &tool = mainToolWidgets[i];
        ClickableWidget *toolbox =
            createToolbox(tool.tool, tool.description, tool.requiresDevice);
        int row = i / 3;
        int col = i % 3;
        m_gridLayout->addWidget(toolbox, row, col);
    }

    contentLayout->addWidget(mainToolsWidget);

    // More Tools Section
    QLabel *moreToolsLabel = new QLabel("More Tools");
    moreToolsLabel->setStyleSheet(
        "font-weight: bold; font-size: 14px; margin-left: 10px");
    contentLayout->addWidget(moreToolsLabel);

    QWidget *moreToolsWidget = new QWidget();
    QGridLayout *moreGridLayout = new QGridLayout(moreToolsWidget);
    moreGridLayout->setSpacing(10);

    QList<iDescriptorToolWidget> moreToolWidgets;
    moreToolWidgets.append(
        {iDescriptorTool::MountDevImage,
         "Mount a compatible device image with a single click", true, ""});
    moreToolWidgets.append(
        {iDescriptorTool::Restart, "Restart device services", true, ""});
    moreToolWidgets.append(
        {iDescriptorTool::Shutdown, "Shut down the device", true, ""});
    moreToolWidgets.append({iDescriptorTool::RecoveryMode,
                            "Enter device recovery mode", true, ""});

    for (int i = 0; i < moreToolWidgets.size(); ++i) {
        const auto &tool = moreToolWidgets[i];
        ClickableWidget *toolbox =
            createToolbox(tool.tool, tool.description, tool.requiresDevice);
        int row = i / 3;
        int col = i % 3;
        moreGridLayout->addWidget(toolbox, row, col);
    }

    contentLayout->addWidget(moreToolsWidget);
    contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolboxWidget::onDeviceSelectionChanged);
}

ClickableWidget *ToolboxWidget::createToolbox(iDescriptorTool tool,
                                              const QString &description,
                                              bool requiresDevice)
{
    ClickableWidget *b = new ClickableWidget();
    b->setStyleSheet("padding: 5px; border: none; outline: none;");

    QVBoxLayout *layout = new QVBoxLayout(b);

    // Icon
    QLabel *iconLabel = new QLabel();
    QIcon icon =
        // TODO:icons
        this->style()->standardIcon(
            static_cast<QStyle::StandardPixmap>(QStyle::SP_DialogOkButton));
    iconLabel->setPixmap(icon.pixmap(32, 32));
    iconLabel->setAlignment(Qt::AlignCenter);
    QString title;
    switch (tool) {
    case iDescriptorTool::Airplayer:
        title = "Airplayer";
        break;
    case iDescriptorTool::RealtimeScreen:
        title = "Realtime Screen";
        break;
    case iDescriptorTool::MountDevImage:
        title = "Mount Dev Image";
        break;
    case iDescriptorTool::VirtualLocation:
        title = "Virtual Location";
        break;
    case iDescriptorTool::Restart:
        title = "Restart";
        break;
    case iDescriptorTool::Shutdown:
        title = "Shutdown";
        break;
    case iDescriptorTool::RecoveryMode:
        title = "Recovery Mode";
        break;
    case iDescriptorTool::QueryMobileGestalt:
        title = "Query Mobile Gestalt";
        break;
    case iDescriptorTool::DeveloperDiskImages:
        title = "Dev Disk Images";
        break;
    case iDescriptorTool::WirelessPhotoImport:
        title = "Wireless Photo Import";
        break;
    case iDescriptorTool::iFuse:
        title = "iFuse Mount";
        break;
    case iDescriptorTool::CableInfoWidget:
        title = "Cable Info";
        break;
    case iDescriptorTool::NetworkDevices:
        title = "Network Devices";
        break;
    default:
        title = "Unknown Tool";
        break;
    }
    // Title
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);

    // Description
    QLabel *descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color: #666; font-size: 12px;");

    layout->addWidget(iconLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);

    b->setCursor(Qt::PointingHandCursor);

    m_toolboxes.append(b);
    m_requiresDevice.append(requiresDevice);
    connect(b, &ClickableWidget::clicked,
            [this, tool]() { onToolboxClicked(tool); });
    return b;
}

void ToolboxWidget::updateDeviceList()
{
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();

    QList<iDescriptorDevice *> devices =
        AppContext::sharedInstance()->getAllDevices();

    if (devices.isEmpty()) {
        m_deviceCombo->addItem("No device connected");
        m_deviceCombo->setEnabled(false);
    } else {
        m_deviceCombo->setEnabled(true);
        for (iDescriptorDevice *device : devices) {
            QString shortUdid =
                QString::fromStdString(device->udid).left(8) + "...";
            m_deviceCombo->addItem(
                QString::fromStdString(device->deviceInfo.productType) + " / " +
                    shortUdid,
                QString::fromStdString(device->udid));
        }
    }

    // After rebuilding the list, explicitly sync the UI to match the
    // state from AppContext. This avoids creating a feedback loop.
    onCurrentDeviceChanged(
        AppContext::sharedInstance()->getCurrentDeviceSelection());

    m_deviceCombo->blockSignals(false);
}

void ToolboxWidget::updateToolboxStates()
{
    bool hasDevice = !AppContext::sharedInstance()->getAllDevices().isEmpty();

    for (int i = 0; i < m_toolboxes.size(); ++i) {
        QWidget *toolbox = m_toolboxes[i];
        bool requiresDevice = m_requiresDevice[i];

        bool enabled = !requiresDevice || hasDevice;
        toolbox->setEnabled(enabled);

        if (enabled) {
            toolbox->setStyleSheet("#toolboxFrame { "
                                   "border-radius: 5px; padding: 5px; }");
        } else {
            toolbox->setStyleSheet("#toolboxFrame { border-radius: 5px; "
                                   "padding: 5px;"
                                   "opacity: 0.45;  }");
        }
    }
}

void ToolboxWidget::updateUI()
{
    updateDeviceList();
    updateToolboxStates();
}

void ToolboxWidget::onDeviceSelectionChanged()
{
    QString selectedUdid = m_deviceCombo->currentData().toString();
    if (selectedUdid.isEmpty()) {
        return;
    }

    // Update the selected device in main menu
    AppContext::sharedInstance()->setCurrentDeviceSelection(
        DeviceSelection(selectedUdid.toStdString()));
}

void ToolboxWidget::onCurrentDeviceChanged(const DeviceSelection &selection)
{
    if (selection.type == DeviceSelection::Normal) {
        int index =
            m_deviceCombo->findData(QString::fromStdString(selection.uuid));
        if (index != -1) {
            // Block signals to prevent recursive calls when we update the UI
            m_deviceCombo->blockSignals(true);
            m_deviceCombo->setCurrentIndex(index);
            m_deviceCombo->blockSignals(false);

            m_uuid = selection.uuid;
            m_currentDevice =
                AppContext::sharedInstance()->getDevice(selection.uuid);
        }
    } else {
        // Handle recovery, pending, or no device selection
        m_uuid.clear();
        m_currentDevice = nullptr;
    }
}

void ToolboxWidget::onToolboxClicked(iDescriptorTool tool)
{

    switch (tool) {
    case iDescriptorTool::Airplayer: {
        if (!m_airplayWindow) {
            m_airplayWindow = new AirPlayWindow();
            connect(m_airplayWindow, &QObject::destroyed, this,
                    [this]() { m_airplayWindow = nullptr; });
            m_airplayWindow->setAttribute(Qt::WA_DeleteOnClose);
            m_airplayWindow->setWindowFlag(Qt::Window);
            m_airplayWindow->resize(400, 300);
            m_airplayWindow->show();
        } else {
            m_airplayWindow->raise();
            m_airplayWindow->activateWindow();
        }
    } break;

    case iDescriptorTool::RealtimeScreen: {
        RealtimeScreenWidget *realtimeScreen =
            new RealtimeScreenWidget(m_currentDevice);
        realtimeScreen->setAttribute(Qt::WA_DeleteOnClose);
        realtimeScreen->show();
    } break;
    case iDescriptorTool::RecoveryMode: {
        // Handle entering recovery mode
        bool success = enterRecoveryMode(m_currentDevice);
        QMessageBox msgBox;
        msgBox.setWindowTitle("Recovery Mode");
        if (success) {
            msgBox.setText("Successfully entered recovery mode.");
        } else {
            msgBox.setText("Failed to enter recovery mode.");
        }
        msgBox.exec();
    } break;
    case iDescriptorTool::MountDevImage: {
        GetMountedImageResult result =
            DevDiskManager::sharedInstance()->getMountedImage(
                m_currentDevice->udid.c_str());

        if (!result.success) {
            QMessageBox::warning(this, "Failure", result.message.c_str());
            return;
        }

        if (result.success && result.sig.empty()) {
            bool devImgSuccess =
                DevDiskManager::sharedInstance()->mountCompatibleImage(
                    m_currentDevice);
            if (!devImgSuccess) {
                QMessageBox::warning(
                    this, "Failure",
                    "Failed to mount developer image on device. "
                    "Try with a different cable.");
                qDebug()
                    << "Failed to mount developer image on device. Cannot set "
                       "location.";
                return;
            }
        }

        QMessageBox::information(
            this, "Success",
            QString("There is already a developer image mounted on device %1.")
                .arg(QString::fromStdString(
                    m_currentDevice->deviceInfo.productType)));

    } break;
    case iDescriptorTool::VirtualLocation: {
        // Handle virtual location functionality
        VirtualLocation *virtualLocation = new VirtualLocation(m_currentDevice);
        virtualLocation->setAttribute(Qt::WA_DeleteOnClose);
        virtualLocation->setWindowFlag(Qt::Window);
        virtualLocation->resize(800, 600);
        virtualLocation->show();
    } break;
    case iDescriptorTool::Restart: {
        restartDevice(m_currentDevice);
    } break;
    case iDescriptorTool::Shutdown: {
        shutdownDevice(m_currentDevice);
    } break;
    case iDescriptorTool::QueryMobileGestalt: {
        // Handle querying MobileGestalt
        QueryMobileGestaltWidget *queryMobileGestaltWidget =
            new QueryMobileGestaltWidget(m_currentDevice);
        queryMobileGestaltWidget->setAttribute(Qt::WA_DeleteOnClose);
        queryMobileGestaltWidget->setWindowFlag(Qt::Window);
        queryMobileGestaltWidget->resize(800, 600);
        queryMobileGestaltWidget->show();
    } break;
    case iDescriptorTool::DeveloperDiskImages: {
        if (!m_devDiskImagesWidget) {
            m_devDiskImagesWidget = new DevDiskImagesWidget(m_currentDevice);
            m_devDiskImagesWidget->setAttribute(Qt::WA_DeleteOnClose);
            m_devDiskImagesWidget->setWindowFlag(Qt::Window);
            m_devDiskImagesWidget->resize(800, 600);
            connect(m_devDiskImagesWidget, &QObject::destroyed, this,
                    [this]() { m_devDiskImagesWidget = nullptr; });
            m_devDiskImagesWidget->show();
        } else {
            m_devDiskImagesWidget->raise();
            m_devDiskImagesWidget->activateWindow();
        }
    } break;
    case iDescriptorTool::WirelessPhotoImport: {
        if (!m_wirelessPhotoImportWidget) {
            m_wirelessPhotoImportWidget = new WirelessPhotoImportWidget();
            connect(m_wirelessPhotoImportWidget, &QObject::destroyed, this,
                    [this]() { m_wirelessPhotoImportWidget = nullptr; });
            m_wirelessPhotoImportWidget->setAttribute(Qt::WA_DeleteOnClose);
            m_wirelessPhotoImportWidget->setWindowFlag(Qt::Window);
            // m_wirelessPhotoImportWidget->resize(800, 600);
            m_wirelessPhotoImportWidget->show();
        } else {
            m_wirelessPhotoImportWidget->raise();
            m_wirelessPhotoImportWidget->activateWindow();
        }
    } break;
#ifndef __APPLE__
    case iDescriptorTool::iFuse: {
        if (!m_ifuseWidget) {
            m_ifuseWidget = new iFuseWidget(m_currentDevice);
            qDebug() << "Created iFuseWidget"
                     << m_currentDevice->deviceInfo.productType.c_str();
            m_ifuseWidget->setAttribute(Qt::WA_DeleteOnClose);
            connect(m_ifuseWidget, &QObject::destroyed, this,
                    [this]() { m_ifuseWidget = nullptr; });
            m_ifuseWidget->setWindowFlag(Qt::Window);
            m_ifuseWidget->resize(600, 400);
            m_ifuseWidget->show();
        } else {
            m_ifuseWidget->raise();
            m_ifuseWidget->activateWindow();
        }
    } break;
#endif
    case iDescriptorTool::CableInfoWidget: {
        CableInfoWidget *cableInfoWidget = new CableInfoWidget(m_currentDevice);
        cableInfoWidget->setAttribute(Qt::WA_DeleteOnClose);
        cableInfoWidget->setWindowFlag(Qt::Window);
        cableInfoWidget->resize(600, 400);
        cableInfoWidget->show();
    } break;
    case iDescriptorTool::NetworkDevices: {
        if (!m_networkDevicesWidget) {
            m_networkDevicesWidget = new NetworkDevicesWidget();
            m_networkDevicesWidget->setAttribute(Qt::WA_DeleteOnClose);
            m_networkDevicesWidget->setWindowFlag(Qt::Window);
            m_networkDevicesWidget->resize(500, 600);
            connect(m_networkDevicesWidget, &QObject::destroyed, this,
                    [this]() { m_networkDevicesWidget = nullptr; });
            m_networkDevicesWidget->show();
        } else {
            m_networkDevicesWidget->raise();
            m_networkDevicesWidget->activateWindow();
        }
    } break;
    default:
        qDebug() << "Clicked on unimplemented tool";
        break;
    }
}

void ToolboxWidget::restartDevice(iDescriptorDevice *device)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Restart Device");
    msgBox.setText("Are you sure you want to restart the device?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    int ret = msgBox.exec();
    if (ret != QMessageBox::Yes)
        return;
    if (!device || device->udid.empty()) {
        return;
    }

    if (!(restart(device->udid)))
        warn("Failed to restart device");
    else {
        warn("Device will restart once unplugged", "Success");
        qDebug() << "Restarting device";
    }
}

void ToolboxWidget::shutdownDevice(iDescriptorDevice *device)
{

    QMessageBox msgBox;
    msgBox.setWindowTitle("Shutdown Device");
    msgBox.setText("Are you sure you want to shutdown the device?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();
    if (ret != QMessageBox::Yes)
        return;
    if (!device || device->udid.empty()) {
        return;
    }

    if (!(shutdown(device->device)))
        // TODO: warn is a safe wrapper for QMessageBox but do we actually need
        // it ?
        warn("Failed to shutdown device");
    else {
        warn("Device will shutdown once unplugged", "Success");
        qDebug() << "Shutting down device";
    }
}

void ToolboxWidget::_enterRecoveryMode(iDescriptorDevice *device)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Enter Recovery Mode");
    msgBox.setText("Are you sure you want to enter recovery mode?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int ret = msgBox.exec();
    if (ret != QMessageBox::Yes)
        return;

    if (!device || device->udid.empty()) {
        return;
    }

    bool success = enterRecoveryMode(device);
    QMessageBox _msgBox;
    _msgBox.setWindowTitle("Recovery Mode");
    if (success) {
        _msgBox.setText("Successfully entered recovery mode.");
    } else {
        _msgBox.setText("Failed to enter recovery mode.");
    }
    _msgBox.exec();
}