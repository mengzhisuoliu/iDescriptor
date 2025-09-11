#include "toolboxwidget.h"
#include "airplaywindow.h"
#include "appcontext.h"
#include "devdiskimageswidget.h"
#include "iDescriptor.h"
#include "querymobilegestaltwidget.h"
#include "realtimescreen.h"
#include "virtual_location.h"
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QStyle>

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

    // Device selection section
    QHBoxLayout *deviceLayout = new QHBoxLayout();
    m_deviceLabel = new QLabel("Device:");
    m_deviceCombo = new QComboBox();
    m_deviceCombo->setMinimumWidth(200);

    deviceLayout->addWidget(m_deviceLabel);
    deviceLayout->addWidget(m_deviceCombo);
    deviceLayout->addStretch();

    mainLayout->addLayout(deviceLayout);

    // Scroll area for toolboxes
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_contentWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_contentWidget);
    m_gridLayout->setSpacing(10);

    // Create toolboxes
    QWidget *airplayerBox =
        createToolbox("Airplayer",
                      "Start an airplayer service to cast your device screen "
                      "(does not require a device to be connected)",
                      "SP_MediaPlay", false);
    QWidget *virtualLocationBox = createToolbox(
        "Virtual Location", "Simulate GPS location on your device",
        "SP_DriveNetIcon", true);
    QWidget *realtimeScreenBox = createToolbox(
        "Realtime Screen",
        "View device screen in real-time (wired connection required)",
        "SP_ComputerIcon", true);
    QWidget *restartBox = createToolbox("Restart", "Restart device services",
                                        "SP_MediaStop", true);
    QWidget *shutdownBox = createToolbox("Shutdown", "Shut down the device",
                                         "SP_MessageBoxWarning", true);
    QWidget *recoveryBox =
        createToolbox("Recovery Mode", "Enter device recovery mode",
                      "SP_MessageBoxWarning", true);
    QWidget *mobileGestaltBox = createToolbox(
        "Query MobileGestalt", "Query device hardware information",
        "SP_FileDialogDetailedView", true);
    QWidget *touchIdBox =
        createToolbox("Touch ID Test", "Test Touch ID functionality",
                      "SP_DialogOkButton", true);
    QWidget *faceIdBox =
        createToolbox("Face ID Test", "Test Face ID functionality",
                      "SP_DialogOkButton", true);
    QWidget *unmountDevImage =
        createToolbox("Unmount Dev Image", "Unmount a developer image",
                      "SP_DialogOkButton", true);
    QWidget *enterRecoveryMode =
        createToolbox("Enter Recovery Mode", "Enter device recovery mode",
                      "SP_DialogOkButton", true);

    QWidget *devDiskImages =
        createToolbox("Developer Disk Images", "Manage developer disk images",
                      "SP_DialogOkButton", false);

    // Add toolboxes to grid (3 columns)
    m_gridLayout->addWidget(airplayerBox, 0, 0);
    m_gridLayout->addWidget(virtualLocationBox, 0, 1);
    m_gridLayout->addWidget(realtimeScreenBox, 0, 2);
    m_gridLayout->addWidget(restartBox, 1, 0);
    m_gridLayout->addWidget(shutdownBox, 1, 1);
    m_gridLayout->addWidget(recoveryBox, 1, 2);
    m_gridLayout->addWidget(mobileGestaltBox, 2, 0);
    m_gridLayout->addWidget(touchIdBox, 2, 1);
    m_gridLayout->addWidget(faceIdBox, 2, 2);
    m_gridLayout->addWidget(enterRecoveryMode, 3, 0);
    m_gridLayout->addWidget(unmountDevImage, 3, 1);
    m_gridLayout->addWidget(devDiskImages, 3, 2);

    m_gridLayout->setRowStretch(3, 1);

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolboxWidget::onDeviceSelectionChanged);
}

QWidget *ToolboxWidget::createToolbox(const QString &title,
                                      const QString &description,
                                      const QString &iconName,
                                      bool requiresDevice)
{
    QFrame *frame = new QFrame();
    frame->setObjectName("toolboxFrame");
    frame->setFrameStyle(QFrame::Box);
    frame->setStyleSheet("#toolboxFrame { border: 1px solid #ccc; "
                         "border-radius: 5px; padding: 5px; }");
    frame->setFixedSize(200, 120);

    QVBoxLayout *layout = new QVBoxLayout(frame);

    // Icon
    QLabel *iconLabel = new QLabel();
    QIcon icon =
        this->style()->standardIcon(static_cast<QStyle::StandardPixmap>(
            iconName == "SP_MediaPlay"           ? QStyle::SP_MediaPlay
            : iconName == "SP_DriveNetIcon"      ? QStyle::SP_DriveNetIcon
            : iconName == "SP_ComputerIcon"      ? QStyle::SP_ComputerIcon
            : iconName == "SP_BrowserReload"     ? QStyle::SP_BrowserReload
            : iconName == "SP_MediaStop"         ? QStyle::SP_MediaStop
            : iconName == "SP_MessageBoxWarning" ? QStyle::SP_MessageBoxWarning
            : iconName == "SP_FileDialogDetailedView"
                ? QStyle::SP_FileDialogDetailedView
                : QStyle::SP_DialogOkButton));
    iconLabel->setPixmap(icon.pixmap(32, 32));
    iconLabel->setAlignment(Qt::AlignCenter);

    // Title
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setFont(QFont("Arial", 10, QFont::Bold));
    titleLabel->setAlignment(Qt::AlignCenter);

    // Description
    QLabel *descLabel = new QLabel(description);
    descLabel->setFont(QFont("Arial", 8));
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color: #666;");

    layout->addWidget(iconLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);

    // Make frame clickable
    frame->setProperty("toolName", title);
    frame->installEventFilter(this);
    frame->setCursor(Qt::PointingHandCursor);

    m_toolboxes.append(frame);
    m_requiresDevice.append(requiresDevice);

    return frame;
}

bool ToolboxWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QFrame *frame = qobject_cast<QFrame *>(obj);
        if (frame && frame->isEnabled()) {
            QString toolName = frame->property("toolName").toString();
            onToolboxClicked(toolName);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
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
        // DO NOT USE AI HARD TODO:
        // Set m_uuid to first device if available
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
            toolbox->setStyleSheet("#toolboxFrame { border: 1px solid #ccc; "
                                   "border-radius: 5px; padding: 5px; }");
        } else {
            toolbox->setStyleSheet(
                "#toolboxFrame { border: 1px solid #ccc; border-radius: 5px; "
                "padding: "
                "5px; background-color: #f0f0f0; color: #999; }");
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

void ToolboxWidget::onToolboxClicked(const QString &toolName)
{
    qDebug() << "Toolbox clicked:" << toolName;

    if (toolName == "Airplayer") {
        AirPlayWindow *airplayWindow = new AirPlayWindow();
        airplayWindow->show();
    } else if (toolName == "Realtime Screen") {
        RealtimeScreen *realtimeScreen =
            new RealtimeScreen(QString::fromStdString(m_uuid));
        realtimeScreen->show();
    } else if (toolName == "Enter Recovery Mode") {
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
    } else if (toolName == "Mount Dev Image") {
        // Handle mounting device image
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
        // msgBox.exec();
    } else if (toolName == "Virtual Location") {
        // Handle virtual location functionality
        VirtualLocation *virtualLocation = new VirtualLocation(m_currentDevice);
        virtualLocation->setAttribute(
            Qt::WA_DeleteOnClose);                  // Optional: auto cleanup
        virtualLocation->setWindowFlag(Qt::Window); // Make it a true window
        virtualLocation->resize(800, 600);          // Optional: default size
        virtualLocation->show();
    } else if (toolName == "Restart") {
        if (!(restart(m_currentDevice->device)))
            warn("Failed to restart device");
        qDebug() << "Restarting device services...";
    } else if (toolName == "Shutdown") {
        // TODO
        //  if (!(shutdown(m_currentDevice->device)))
        //      warn("Failed to shutdown device");
        qDebug() << "Shutting down device...";
    } else if (toolName == "Recovery Mode") {
        // Handle entering recovery mode
        // bool success = enterRecoveryMode(m_currentDevice);
        // QMessageBox msgBox;
        // msgBox.setWindowTitle("Recovery Mode");
        // if (success)
        // {
        //     msgBox.setText("Successfully entered recovery mode.");
        // }
        // else
        // {
        //     msgBox.setText("Failed to enter recovery mode.");
        // }
        // msgBox.exec();
    } else if (toolName == "Query MobileGestalt") {
        // Handle querying MobileGestalt
        QueryMobileGestaltWidget *queryMobileGestaltWidget =
            new QueryMobileGestaltWidget();
        queryMobileGestaltWidget->show();
    } else if (toolName == "Developer Disk Images") {
        // Handle developer disk images
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
    } else if (toolName == "Touch ID Test") {
        // Handle Touch ID test
        QMessageBox::information(
            this, "Touch ID Test",
            "Touch ID test functionality not implemented.");
    } else if (toolName == "Face ID Test") {
        // Handle Face ID test
        QMessageBox::information(this, "Face ID Test",
                                 "Face ID test functionality not implemented.");
    }
    // Implement specific tool functionality here
}
