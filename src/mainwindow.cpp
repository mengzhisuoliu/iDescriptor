#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "customtabwidget.h"
#include "detailwindow.h"
#include "ifusediskunmountbutton.h"
#include "ifusemanager.h"
#include "settingswidget.h"

#include "appswidget.h"
#include "devicemanagerwidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "jailbrokenwidget.h"
#include "libirecovery.h"
#include "toolboxwidget.h"
#include "welcomewidget.h"
#include <QHBoxLayout>
#include <QStack>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <unistd.h>

#include "appcontext.h"
#include "settingsmanager.h"
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

#ifdef WIN32
#include "platform/windows/diagnose_widget.h"
#endif

void handleCallback(const idevice_event_t *event, void *userData)
{
    printf("Device event received: ");

    switch (event->event) {
    case IDEVICE_DEVICE_ADD: {
        /* this should never happen iDescriptor does not support network devices
        but for some reason even though we are only listening for USB devices,
        we still get network devices on macOS*/
        if (event->conn_type == CONNECTION_NETWORK) {
            return;
        }
        qDebug() << "Device added: " << QString::fromUtf8(event->udid);

        QMetaObject::invokeMethod(
            AppContext::sharedInstance(), "addDevice", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(event->udid)),
            Q_ARG(idevice_connection_type, event->conn_type),
            Q_ARG(AddType, AddType::Regular));
        break;
    }

    case IDEVICE_DEVICE_REMOVE: {
        QMetaObject::invokeMethod(AppContext::sharedInstance(), "removeDevice",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, QString(event->udid)));
        break;
    }

    case IDEVICE_DEVICE_PAIRED: {
        if (event->conn_type == CONNECTION_NETWORK) {
            warn("Network devices are not supported but a network device was "
                 "received in event listener. Please report this issue.");
            return;
        }
        qDebug() << "Device paired: " << QString::fromUtf8(event->udid);

        QMetaObject::invokeMethod(
            AppContext::sharedInstance(), "addDevice", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(event->udid)),
            Q_ARG(idevice_connection_type, event->conn_type),
            Q_ARG(AddType, AddType::Pairing));
        break;
    }
    default:
        qDebug() << "Unhandled event: " << event->event;
    }
    // return;
}

void handleCallbackRecovery(const irecv_device_event_t *event, void *userData)
{

    switch (event->type) {
    case IRECV_DEVICE_ADD:
        qDebug() << "Recovery device added: ";
        QMetaObject::invokeMethod(AppContext::sharedInstance(),
                                  "addRecoveryDevice", Qt::QueuedConnection,
                                  Q_ARG(uint64_t, event->device_info->ecid));
        break;
    case IRECV_DEVICE_REMOVE:
        qDebug() << "Recovery device removed: ";
        QMetaObject::invokeMethod(AppContext::sharedInstance(),
                                  "removeRecoveryDevice", Qt::QueuedConnection,
                                  Q_ARG(uint64_t, event->device_info->ecid));
        break;
    default:
        printf("Unhandled recovery event: %d\n", event->type);
    }
}
irecv_device_event_context_t context;

MainWindow *MainWindow::sharedInstance()
{
    static MainWindow instance;
    return &instance;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    const QSize minSize(900, 600);
    setMinimumSize(minSize);
    resize(minSize);
    // TODO
    // setWindowIcon(QIcon(":/resources/icons/icon.png"));

    // Create custom tab widget
    m_customTabWidget = new CustomTabWidget(this);
    m_customTabWidget->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea,
                                    false);

    setContentsMargins(0, 0, 0, 0);
#ifdef __APPLE__
    setupMacOSWindow(this);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif
    setCentralWidget(m_customTabWidget);

    // Create device manager and stacked widget for main tab
    m_mainStackedWidget = new QStackedWidget();

    // Welcome page (shown when no devices are connected)
    WelcomeWidget *welcomePage = new WelcomeWidget(this);
    // No devices page
    QWidget *noDevicesPage = new QWidget();
    QVBoxLayout *noDeviceLayout = new QVBoxLayout(noDevicesPage);
    noDeviceLayout->addStretch();
    QHBoxLayout *labelLayout = new QHBoxLayout();
    labelLayout->addStretch();
    QLabel *noDeviceLabel = new QLabel("No devices detected");
    noDeviceLabel->setAlignment(Qt::AlignCenter);
    labelLayout->addWidget(noDeviceLabel);
    labelLayout->addStretch();
    noDeviceLayout->addLayout(labelLayout);

#ifdef WIN32
    // Add diagnose widget to check dependencies
    // DiagnoseWidget *diagnoseWidget = new DiagnoseWidget();
    // noDeviceLayout->addWidget(diagnoseWidget);
#endif

    noDeviceLayout->addStretch();

    m_deviceManager = new DeviceManagerWidget(this);

    m_mainStackedWidget->addWidget(welcomePage);
    m_mainStackedWidget->addWidget(m_deviceManager);

    connect(m_deviceManager, &DeviceManagerWidget::updateNoDevicesConnected,
            this, &MainWindow::updateNoDevicesConnected);

    // Add tabs with icons
    QIcon deviceIcon(":/resources/icons/MdiLightningBolt.png");
    m_customTabWidget->addTab(m_mainStackedWidget, deviceIcon, "iDevice");
    m_customTabWidget->addTab(AppsWidget::sharedInstance(), "Apps");
    m_customTabWidget->addTab(new ToolboxWidget(this), "Toolbox");

    auto *jailbrokenWidget = new JailbrokenWidget(this);
    m_customTabWidget->addTab(jailbrokenWidget, "Jailbroken");
    m_customTabWidget->finalizeStyles();

    // connect(
    //     m_customTabWidget, &CustomTabWidget::currentChanged, this,
    //     [this, jailbrokenWidget](int index) {
    //         if (index == 3) { // Jailbroken tab
    //             jailbrokenWidget->initWidget();
    //         }
    //     },
    //     Qt::SingleShotConnection);

    // settings button
    ZIconWidget *settingsButton = new ZIconWidget(
        QIcon(":/resources/icons/MingcuteSettings7Line.png"), "Settings");
    settingsButton->setCursor(Qt::PointingHandCursor);
    settingsButton->setFixedSize(24, 24);
    connect(settingsButton, &ZIconWidget::clicked, this, [this]() {
        SettingsManager::sharedInstance()->showSettingsDialog();
    });

    ZIconWidget *githubButton = new ZIconWidget(
        QIcon(":/resources/icons/MdiGithub.png"), "iDescriptor on GitHub");
    githubButton->setCursor(Qt::PointingHandCursor);
    githubButton->setFixedSize(24, 24);
    connect(githubButton, &ZIconWidget::clicked, this, []() {
        QDesktopServices::openUrl(
            QUrl("https://github.com/uncor3/iDescriptor"));
    });

    m_connectedDeviceCountLabel = new QLabel("iDescriptor: no devices");
    m_connectedDeviceCountLabel->setContentsMargins(5, 0, 5, 0);
    m_connectedDeviceCountLabel->setStyleSheet(
        "QLabel:hover { background-color : #13131319; }");

    ui->statusbar->addWidget(m_connectedDeviceCountLabel);
    ui->statusbar->setContentsMargins(0, 0, 0, 0);
    ui->statusbar->addPermanentWidget(settingsButton);
    ui->statusbar->addPermanentWidget(githubButton);

#ifdef __linux__
    QList<QString> mounted_iFusePaths = iFuseManager::getMountPoints();

    for (const QString &path : mounted_iFusePaths) {
        auto *p = new iFuseDiskUnmountButton(path);

        ui->statusbar->addPermanentWidget(p);
        connect(p, &iFuseDiskUnmountButton::clicked, this, [this, p, path]() {
            bool ok = iFuseManager::linuxUnmount(path);
            if (!ok) {
                QMessageBox::warning(nullptr, "Unmount Failed",
                                     "Failed to unmount iFuse at " + path +
                                         ". Please try again.");
                return;
            }
            ui->statusbar->removeWidget(p);
            p->deleteLater();
        });
    }
#endif

    irecv_error_t res_recovery =
        irecv_device_event_subscribe(&context, handleCallbackRecovery, nullptr);

    if (res_recovery != IRECV_E_SUCCESS) {
        qDebug() << "ERROR: Unable to subscribe to recovery device events. "
                    "Error code:"
                 << res_recovery;
    }
    qDebug() << "Subscribed to recovery device events successfully.";

    idevice_error_t res = idevice_event_subscribe(handleCallback, nullptr);
    if (res != IDEVICE_E_SUCCESS) {
        qDebug() << "ERROR: Unable to subscribe to device events. Error code:"
                 << res;
    }
    qDebug() << "Subscribed to device events successfully.";
    createMenus();
    // Example usage with customization

    UpdateProcedure updateProcedure;

    switch (ZUpdater::detectPlatform()) {
    // todo: adjust for portable
    case Platform::Windows:
        updateProcedure = UpdateProcedure{
            true,
            false,
            true,
            "The application will now quit to install the update.",
            "Do you want to install the downloaded update now?",
        };
        break;
        // todo: adjust for pkg managers
    case Platform::MacOS:
        updateProcedure = UpdateProcedure{
            false,
            false,
            true,
            "The application will now quit to install the update.",
            "Do you want to install the downloaded update now?",
        };
        break;
        // todo: adjust for pkg managers
    case Platform::Linux:
        updateProcedure = UpdateProcedure{
            false,
            true,
            false,
            "There is new update available",
            "Would you like to download it now?",
        };
        break;
    default:
        updateProcedure = UpdateProcedure{
            false, false, false, "", "",
        };
    }

    m_updater = new ZUpdater("uncor3/libtest", APP_VERSION, "iDescriptor",
                             updateProcedure,
                             false, // isPortable - set to true if running
                                    // portable version on Windows
                             false, // isPackageManaged - set to true if
                                    // installed via package manager on Linux
                             this);
    qDebug() << "Checking for updates...";
    m_updater->checkForUpdates();
}

void MainWindow::createMenus()
{
#ifdef Q_OS_MAC
    QMenu *actionsMenu = menuBar()->addMenu("&Actions");

    QAction *aboutAct = new QAction("&About iDescriptor", this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "iDescriptor",
                           "A free and open-source idevice management tool.");
    });
    actionsMenu->addAction(aboutAct);
#endif
}

void MainWindow::updateNoDevicesConnected()
{
    qDebug() << "Is there no devices connected? "
             << AppContext::sharedInstance()->noDevicesConnected();
    if (AppContext::sharedInstance()->noDevicesConnected()) {

        m_connectedDeviceCountLabel->setText("iDescriptor: no devices");
        return m_mainStackedWidget->setCurrentIndex(0); // Show Welcome page
    }
    int deviceCount = AppContext::sharedInstance()->getConnectedDeviceCount();
    m_connectedDeviceCountLabel->setText(
        "iDescriptor: " + QString::number(deviceCount) +
        (deviceCount == 1 ? " device" : " devices") + " connected");
    m_mainStackedWidget->setCurrentIndex(1); // Show device list page
}

MainWindow::~MainWindow()
{
    idevice_event_unsubscribe();
    irecv_device_event_unsubscribe(context);
    delete ui;
    delete m_updater;
    sleep(2); // Give some time for cleanup to finish
}
