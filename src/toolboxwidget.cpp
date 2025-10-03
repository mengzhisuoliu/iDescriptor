#include "toolboxwidget.h"
#include "airplaywindow.h"
#include "appcontext.h"
#include "cableinfowidget.h"
#include "devdiskimageswidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "ifusewidget.h"
#include "pcfileexplorerwidget.h"
#include "querymobilegestaltwidget.h"
#include "realtimescreen.h"
#include "virtual_location.h"
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
        (ldret = lockdownd_client_new(device->device, &client,
                                      "EnterRecoveryMode"))) {
        printf("ERROR: Could not connect to lockdownd: %s (%d)\n",
               lockdownd_strerror(ldret), ldret);
        // idevice_free(device);
        return 1;
    }

    int res = 0;
    // printf("Telling device with udid %s to enter recovery mode.\n", udid);
    ldret = lockdownd_enter_recovery(client);
    if (ldret == LOCKDOWN_E_SESSION_INACTIVE) {
        lockdownd_client_free(client);
        client = NULL;
        if (LOCKDOWN_E_SUCCESS !=
            (ldret = lockdownd_client_new_with_handshake(
                 device->device, &client, "EnterRecoveryMode"))) {
            printf("ERROR: Could not connect to lockdownd: %s (%d)\n",
                   lockdownd_strerror(ldret), ldret);
            // idevice_free(device);
            return 1;
        }
        ldret = lockdownd_enter_recovery(client);
    }
    if (ldret != LOCKDOWN_E_SUCCESS) {
        printf("Failed to enter recovery mode.\n");
        res = 1;
    } else {
        printf("Device is successfully switching to recovery mode.\n");
    }

    lockdownd_client_free(client);
    // idevice_free(device);

    return 0;
}

ToolboxWidget::ToolboxWidget(QWidget *parent) : QWidget{parent}
{
    setupUI();
    updateDeviceList();
    updateToolboxStates();

    // Connect to AppContext signals
    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            &ToolboxWidget::onDeviceAdded);
    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            &ToolboxWidget::onDeviceRemoved);
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
    m_gridLayout = new QGridLayout(m_contentWidget);
    m_gridLayout->setSpacing(10);

    QList<iDescriptorToolWidget> toolWidgets;
    toolWidgets.append({iDescriptorTool::Airplayer,
                        "Start an airplayer service to cast your device screen "
                        "(does not require a device to be connected)",
                        false, ""});
    toolWidgets.append({iDescriptorTool::VirtualLocation,
                        "Simulate GPS location on your device", true, ""});
    toolWidgets.append(
        {iDescriptorTool::RealtimeScreen,
         "View device screen in real-time (wired connection required)", true,
         ""});
    toolWidgets.append(
        {iDescriptorTool::Restart, "Restart device services", true, ""});
    toolWidgets.append(
        {iDescriptorTool::Shutdown, "Shut down the device", true, ""});
    toolWidgets.append({iDescriptorTool::RecoveryMode,
                        "Enter device recovery mode", true, ""});
    toolWidgets.append({iDescriptorTool::QueryMobileGestalt,
                        "Query device hardware information", true, ""});
    toolWidgets.append({iDescriptorTool::TouchIdTest,
                        "Test Touch ID functionality", true, ""});
    toolWidgets.append(
        {iDescriptorTool::FaceIdTest, "Test Face ID functionality", true, ""});
    toolWidgets.append({iDescriptorTool::UnmountDevImage,
                        "Unmount a developer image", true, ""});
    toolWidgets.append({iDescriptorTool::EnterRecoveryMode,
                        "Enter device recovery mode", true, ""});
    toolWidgets.append({iDescriptorTool::DeveloperDiskImages,
                        "Manage developer disk images", false, ""});
    toolWidgets.append({iDescriptorTool::WirelessFileImport,
                        "Import files wirelessly to your iDevice", false, ""});
    toolWidgets.append({iDescriptorTool::MountIphone,
                        "Mount your iPhone's filesystem on your PC", true, ""});
    toolWidgets.append({iDescriptorTool::CableInfoWidget,
                        "View detailed cable and connection info", true, ""});

    for (int i = 0; i < toolWidgets.size(); ++i) {
        const auto &tool = toolWidgets[i];
        ClickableWidget *toolbox =
            createToolbox(tool.tool, tool.description, tool.requiresDevice);
        int row = i / 3;
        int col = i % 3;
        m_gridLayout->addWidget(toolbox, row, col);
    }

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
    case iDescriptorTool::EnterRecoveryMode:
        title = "Enter Recovery Mode";
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
    case iDescriptorTool::WirelessFileImport:
        title = "Wireless File Import";
        break;
    case iDescriptorTool::MountIphone:
        title = "iFuse Mount";
        break;
    case iDescriptorTool::CableInfoWidget:
        title = "Cable Info";
        break;
    case iDescriptorTool::TouchIdTest:
        title = "Touch ID Test";
        break;
    case iDescriptorTool::FaceIdTest:
        title = "Face ID Test";
        break;
    case iDescriptorTool::UnmountDevImage:
        title = "Unmount Dev Image";
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
    m_deviceCombo->clear();

    QList<iDescriptorDevice *> devices =
        AppContext::sharedInstance()->getAllDevices();

    if (devices.isEmpty()) {
        m_deviceCombo->addItem("No device connected");
        m_deviceCombo->setEnabled(false);
        m_uuid.clear(); // No device, clear uuid
    } else {
        m_deviceCombo->setEnabled(true);
        for (iDescriptorDevice *device : devices) {
            m_deviceCombo->addItem(QString::fromStdString(device->udid));
        }
        // TODO:
        m_uuid = devices.first()->udid;
        m_currentDevice = devices.first(); // Set current device to first one
    }
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

void ToolboxWidget::onDeviceAdded()
{
    updateDeviceList();
    updateToolboxStates();
}

void ToolboxWidget::onDeviceRemoved()
{
    updateDeviceList();
    updateToolboxStates();
}

void ToolboxWidget::onDeviceSelectionChanged()
{
    // Handle device selection change
    QString selectedDevice = m_deviceCombo->currentText();
    qDebug() << "Selected device:" << selectedDevice;

    // Update m_uuid if a valid device is selected
    QList<iDescriptorDevice *> devices =
        AppContext::sharedInstance()->getAllDevices();
    for (iDescriptorDevice *device : devices) {
        if (QString::fromStdString(device->udid) == selectedDevice) {
            m_uuid = device->udid;
            return;
        }
    }
    m_uuid.clear(); // No valid device selected
}

void ToolboxWidget::onToolboxClicked(iDescriptorTool tool)
{

    switch (tool) {
    case iDescriptorTool::Airplayer: {
        AirPlayWindow *airplayWindow = new AirPlayWindow();
        airplayWindow->setAttribute(Qt::WA_DeleteOnClose);
        airplayWindow->setWindowFlag(Qt::Window);
        airplayWindow->resize(400, 300);
        airplayWindow->show();
    } break;

    case iDescriptorTool::RealtimeScreen: {
        RealtimeScreen *realtimeScreen =
            new RealtimeScreen(QString::fromStdString(m_uuid));
        realtimeScreen->show();
    } break;
    case iDescriptorTool::EnterRecoveryMode: {
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
        // TODO: Handle mounting device image
        // bool success =
        //     mount_dev_image(const_cast<char
        //     *>(m_currentDevice->udid.c_str()));
        // QMessageBox msgBox;
        // msgBox.setWindowTitle("Mount Dev Image");
        // if (success) {
        //     msgBox.setText("Successfully mounted device image.");
        // } else {
        //     msgBox.setText("Failed to mount device image.");
        // }
    } break;
    case iDescriptorTool::VirtualLocation: {
        // Handle virtual location functionality
        VirtualLocation *virtualLocation = new VirtualLocation(m_currentDevice);
        virtualLocation->setAttribute(
            Qt::WA_DeleteOnClose);                  // Optional: auto cleanup
        virtualLocation->setWindowFlag(Qt::Window); // Make it a true window
        virtualLocation->resize(800, 600);          // Optional: default size
        virtualLocation->show();
    } break;
    case iDescriptorTool::Restart: {
        // TODO:WIP
        std::string udid = m_currentDevice->udid;
        AppContext::sharedInstance()->instanceRemoveDevice(
            QString::fromStdString(udid));
        // QMetaObject::invokeMethod(AppContext::sharedInstance(),
        // "removeDevice",
        //                           Qt::QueuedConnection,
        //                           Q_ARG(QString, QString(udid.c_str())));
        if (!(restart(udid)))
            warn("Failed to restart device");
        else {
            warn("Device services restarted successfully", "Success");
            qDebug() << "Restarting device";
        }
    } break;
    case iDescriptorTool::Shutdown: {
        // TODO
        //  if (!(shutdown(m_currentDevice->device)))
        //      warn("Failed to shutdown device");
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
        // single instance lock
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
    case iDescriptorTool::WirelessFileImport: {
        // Handle wireless file import
        PCFileExplorerWidget *fileExplorer = new PCFileExplorerWidget();
        fileExplorer->setAttribute(Qt::WA_DeleteOnClose);
        fileExplorer->setWindowFlag(Qt::Window);
        fileExplorer->resize(800, 600);
        fileExplorer->show();
    } break;
    case iDescriptorTool::MountIphone: {
        iFuseWidget *ifuseWidget = new iFuseWidget(m_currentDevice);
        ifuseWidget->setAttribute(Qt::WA_DeleteOnClose);
        ifuseWidget->setWindowFlag(Qt::Window);
        ifuseWidget->resize(600, 400);
        ifuseWidget->show();
    } break;
    case iDescriptorTool::CableInfoWidget: {
        CableInfoWidget *cableInfoWidget = new CableInfoWidget(m_currentDevice);
        cableInfoWidget->setAttribute(Qt::WA_DeleteOnClose);
        cableInfoWidget->setWindowFlag(Qt::Window);
        cableInfoWidget->resize(600, 400);
        cableInfoWidget->show();
    } break;
    default:
        qDebug() << "Clicked on unimplemented tool";
        break;
    }
}
