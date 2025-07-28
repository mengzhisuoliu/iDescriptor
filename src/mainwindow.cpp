#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "detailwindow.h"
#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QMessageBox>
#include <QSvgRenderer>
#include <QtSvg>

#include <libimobiledevice/libimobiledevice.h>
#include <stdio.h>
#include <unistd.h>

#include "appswidget.h"
#include "devicetabwidget.h"
#include "iDescriptor.h"
#include "libirecovery.h"
#include "toolboxwidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStack>
#include <QVBoxLayout>
#include <QWidget>

#include "appcontext.h"
#include "deviceinfowidget.h"
#include "devicemenuwidget.h"
#include "fileexplorerwidget.h"
#include "jailbrokenwidget.h"
#include "recoverydeviceinfowidget.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>

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
        // TODO: handle recovery device addition
        //  QMetaObject::invokeMethod(ctx->mainWindow, "onRecoveryDeviceAdded",
        //                            Qt::QueuedConnection,
        //                            Q_ARG(QObject *, new
        //                            RecoveryDeviceInfo(event)));
        break;
    case IRECV_DEVICE_REMOVE:
        qDebug() << "Recovery device removed: ";
        QMetaObject::invokeMethod(
            AppContext::sharedInstance(), "onRecoveryDeviceRemoved",
            Qt::QueuedConnection,
            Q_ARG(QString, QString::number(event->device_info->ecid)));
        break;
    default:
        printf("Unhandled recovery event: %d\n", event->type);
    }
}
irecv_device_event_context_t context;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Replace the default tab widget with custom one
    DeviceTabWidget *customTabWidget = new DeviceTabWidget(this);

    // Replace the existing tabWidget in the UI
    QWidget *tabWidgetParent = ui->tabWidget->parentWidget();
    QLayout *parentLayout = tabWidgetParent->layout();

    if (parentLayout) {
        parentLayout->replaceWidget(ui->tabWidget, customTabWidget);
    }

    delete ui->tabWidget;
    ui->tabWidget = customTabWidget;

    ui->mainTabWidget->widget(1)->layout()->addWidget(new AppsWidget(this));
    ui->mainTabWidget->widget(2)->layout()->addWidget(new ToolboxWidget(this));
    ui->mainTabWidget->widget(3)->layout()->addWidget(
        new JailbrokenWidget(this));

    customTabWidget->tabBar()->setMinimumWidth(75);
    customTabWidget->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
    customTabWidget->setStyleSheet("QTabWidget::pane {"
                                   //    "  border: 1px solid #ccc;"
                                   "}"
                                   "QTabBar::tab {"
                                   "  padding: 5px;"
                                   "}");
    // customTabWidget->tabBar()->setMinimumHeight(100);
    irecv_error_t res_recovery =
        irecv_device_event_subscribe(&context, handleCallbackRecovery, nullptr);

    if (res_recovery != IRECV_E_SUCCESS) {
        printf("ERROR: Unable to subscribe to recovery device events.\n");
    }

    idevice_error_t res = idevice_event_subscribe(handleCallback, nullptr);
    if (res != IDEVICE_E_SUCCESS) {
        printf("ERROR: Unable to subscribe to device events.\n");
    }

    connect(
        AppContext::sharedInstance(), &AppContext::deviceAdded, this,
        [this](iDescriptorDevice *device) {
            qDebug() << "Connect ::deviceAdded Adding:"
                     << QString::fromStdString(device->udid);
            // Create device info widget
            DeviceMenuWidget *deviceWidget = new DeviceMenuWidget(device, this);
            m_device_menu_widgets[device->udid] = deviceWidget;
            // Get device icon and product type for tab
            QString tabTitle =
                QString::fromStdString(device->deviceInfo.productType);

            // Add tab with custom icon
            DeviceTabWidget *customTabWidget =
                qobject_cast<DeviceTabWidget *>(ui->tabWidget);
            int mostRecentDevice =
                customTabWidget->addTabCustom(deviceWidget, tabTitle);
            customTabWidget->setSizePolicy(QSizePolicy::Expanding,
                                           QSizePolicy::Preferred);

            updateNoDevicesConnected();

            connect(customTabWidget, &DeviceTabWidget::navigationButtonClicked,
                    this, [this](int tabIndex, const QString &buttonName) {
                        // Get the widget at the specified tab index
                        QWidget *tabWidget = ui->tabWidget->widget(tabIndex);
                        DeviceMenuWidget *deviceMenuWidget =
                            qobject_cast<DeviceMenuWidget *>(tabWidget);

                        if (deviceMenuWidget) {
                            // Call a method to change the internal tab
                            deviceMenuWidget->switchToTab(buttonName);
                        }
                    });
        });

    connect(
        AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
        [this](const std::string &uuid) {
            qDebug() << "Removing:" << QString::fromStdString(uuid);
            DeviceMenuWidget *deviceWidget =
                qobject_cast<DeviceMenuWidget *>(m_device_menu_widgets[uuid]);

            if (deviceWidget) {
                ui->tabWidget->removeTab(ui->tabWidget->indexOf(deviceWidget));
                m_device_menu_widgets.erase(uuid);
                // deviceWidget->deleteLater();
                delete deviceWidget;
            }

            updateNoDevicesConnected();
        });

    connect(
        AppContext::sharedInstance(), &AppContext::devicePairPending, this,
        [this](const QString &udid) {
            QWidget *placeholderWidget = new QWidget();
            QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);
            QLabel *label = new QLabel(
                "Device is not paired. Please pair the device to continue.");
            label->setAlignment(Qt::AlignCenter);
            layout->addWidget(label);
            placeholderWidget->setLayout(layout);
            m_device_menu_widgets[udid.toStdString()] = placeholderWidget;

            // DeviceTabWidget *customTabWidget =
            //     qobject_cast<DeviceTabWidget *>(ui->tabWidget);

            // QString tabTitle = QString::fromStdString(udid.toStdString());
            // QPixmap placeholderIcon(16, 16);
            // placeholderIcon.fill(Qt::red);

            // int mostRecentDevice = customTabWidget->addTabWithIcon(
            //     placeholderWidget, placeholderIcon, tabTitle);
            int mostRecentDevice = ui->tabWidget->addTab(
                placeholderWidget, QIcon(),
                QString::fromStdString(udid.toStdString()));
            // customTabWidget->setSizePolicy(QSizePolicy::Expanding,
            //                                QSizePolicy::Preferred);
            // customTabWidget->setCurrentIndex(mostRecentDevice);
            ui->tabWidget->setCurrentIndex(mostRecentDevice);
            ui->stackedWidget->setCurrentIndex(1); // Show device list page
        });

    connect(AppContext::sharedInstance(), &AppContext::devicePaired, this,
            [this](iDescriptorDevice *device) {
                qDebug() << "Device paired:"
                         << QString::fromStdString(device->udid);

                DeviceMenuWidget *deviceWidget = new DeviceMenuWidget(device);

                // Find the tab index for this device
                int tabIndex = -1;
                for (int i = 0; i < ui->tabWidget->count(); ++i) {
                    if (ui->tabWidget->tabText(i) ==
                        QString::fromStdString(device->udid)) {
                        tabIndex = i;
                        break;
                    }
                }

                // If tab exists, remove the old widget and tab
                if (tabIndex != -1) {
                    QWidget *oldWidget = ui->tabWidget->widget(tabIndex);
                    ui->tabWidget->removeTab(tabIndex);
                    if (oldWidget)
                        oldWidget->deleteLater();
                }

                DeviceTabWidget *customTabWidget =
                    qobject_cast<DeviceTabWidget *>(ui->tabWidget);

                QString tabTitle =
                    QString::fromStdString(device->deviceInfo.productType);

                int mostRecentDevice =
                    customTabWidget->addTabCustom(deviceWidget, tabTitle);
                // int mostRecentDevice = ui->tabWidget->addTab(
                // placeholderWidget, getDeviceIcon(udid.toStdString()),
                // QString::fromStdString(udid.toStdString()));
                customTabWidget->setSizePolicy(QSizePolicy::Expanding,
                                               QSizePolicy::Preferred);
                customTabWidget->setCurrentIndex(mostRecentDevice);
                // ui->tabWidget->setCurrentIndex(mostRecentDevice);
                ui->stackedWidget->setCurrentIndex(1); // Show device list page

                // Clean up old mapping and update
                if (m_device_menu_widgets.count(device->udid)) {
                    m_device_menu_widgets[device->udid]->deleteLater();
                }
                m_device_menu_widgets[device->udid] = deviceWidget;
            });

    connect(
        AppContext::sharedInstance(), &AppContext::recoveryDeviceRemoved, this,
        [this](const QString &ecid) {
            qDebug() << "Removing:" << ecid;
            std::string ecidStr = ecid.toStdString();
            DeviceMenuWidget *deviceWidget = qobject_cast<DeviceMenuWidget *>(
                m_device_menu_widgets[ecidStr]);

            if (deviceWidget) {
                ui->tabWidget->removeTab(ui->tabWidget->indexOf(deviceWidget));
                m_device_menu_widgets.erase(ecidStr);
                delete deviceWidget;
            }
            updateNoDevicesConnected();
        });
}

void MainWindow::updateNoDevicesConnected()
{
    qDebug() << "Is there no devices connected? "
             << AppContext::sharedInstance()->noDevicesConnected();
    if (AppContext::sharedInstance()->noDevicesConnected())
        return ui->stackedWidget->setCurrentIndex(
            0);                            // Show "No Devices Connected" page
    ui->stackedWidget->setCurrentIndex(1); // Show device list page
}

void MainWindow::onRecoveryDeviceAdded(QObject *recoveryDeviceInfoObj)
{
    if (!recoveryDeviceInfoObj)
        // TODO: handle
        return;
    try {
        ui->stackedWidget->setCurrentIndex(1);
        RecoveryDeviceInfo *device =
            qobject_cast<RecoveryDeviceInfo *>(recoveryDeviceInfoObj);
        if (!device) {
            qDebug() << "Invalid recovery device info object";
            return;
        }
        // IDescriptorInitDeviceResultRecovery initResult=
        // init_idescriptor_recovery_device(deviceInfo);

        // IDescriptorInitDeviceResult initResult =
        // init_idescriptor_device(udid.toStdString().c_str());

        qDebug() << "Recovery device initialized: " << device->ecid;

        std::string added_ecid =
            AppContext::sharedInstance()->addRecoveryDevice(device);

        // Create device info widget
        RecoveryDeviceInfoWidget *recoveryDeviceInfoWidget =
            new RecoveryDeviceInfoWidget(device);
        QPixmap recoveryIcon(16, 16);
        recoveryIcon.fill(Qt::transparent);
        QPainter painter(&recoveryIcon);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(255, 59, 48)); // Red for recovery mode
        painter.drawRoundedRect(2, 2, 12, 12, 2, 2);

        // int mostRecentDevice =
        // customTabWidget->addTabWithIcon(recoveryDeviceInfoWidget,
        // recoveryIcon, "Recovery Mode");

        m_device_menu_widgets[added_ecid] = recoveryDeviceInfoWidget;
        // Get device icon and product type for tab
        // QString tabTitle =
        // QString::fromStdString(device->product.toStdString());
        QString tabTitle = QString::fromStdString("recovery mode device");

        // Add tab with custom icon
        DeviceTabWidget *customTabWidget =
            qobject_cast<DeviceTabWidget *>(ui->tabWidget);
        int mostRecentDevice =
            customTabWidget->addTabCustom(recoveryDeviceInfoWidget, tabTitle);
        customTabWidget->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Preferred);

        connect(customTabWidget, &DeviceTabWidget::navigationButtonClicked,
                this, [this](int tabIndex, const QString &buttonName) {
                    // Get the widget at the specified tab index
                    QWidget *tabWidget = ui->tabWidget->widget(tabIndex);
                    DeviceMenuWidget *deviceMenuWidget =
                        qobject_cast<DeviceMenuWidget *>(tabWidget);

                    if (deviceMenuWidget) {
                        // Call a method to change the internal tab
                        deviceMenuWidget->switchToTab(buttonName);
                    }
                });

        ui->tabWidget->setCurrentIndex(mostRecentDevice);
    } catch (const std::exception &e) {
        qDebug() << "Exception in onDeviceAdded: " << e.what();
        QMessageBox::critical(
            this, "Error",
            "An error occurred while processing device information");
    }
}

void MainWindow::onRecoveryDeviceRemoved(QObject *deviceInfoObj)
{
    auto *info = qobject_cast<RecoveryDeviceInfo *>(deviceInfoObj);
    if (!info)
        return;

    qDebug() << "Recovery device removed: " << info->ecid;
    // Find the tab index for the recovery device
    int tabIndex = -1;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        if (ui->tabWidget->tabText(i) ==
            QString::fromStdString("Recovery Mode Device")) {
            tabIndex = i;
            break;
        }
    }
    if (tabIndex != -1) {
        ui->tabWidget->removeTab(tabIndex);
        qDebug() << "Removed tab for recovery device: " << info->ecid;
    }
}

MainWindow::~MainWindow()
{
    idevice_event_unsubscribe();
    irecv_device_event_unsubscribe(context);
    // TODO:Clean up all devices
    // for (unsigned i = 0; i < idescriptor_devices.size(); ++i)
    // {
    //     cleanDevice(idescriptor_devices.at(i));
    // }
    // idescriptor_devices.clear();
    delete ui;
    sleep(1); // Give some time for cleanup to finish
}

void MainWindow::onDeviceInitFailed(QString udid, lockdownd_error_t err)
{
    QString errorTitle = "Device Connection Error";
    QString errorMessage;

    switch (err) {
    case LOCKDOWN_E_PASSWORD_PROTECTED:
        errorMessage =
            QString(
                "Could not validate device %1 because a passcode is set.\n\n"
                "Please enter the passcode on your device and try again.")
                .arg(udid);
        qDebug() << "ERROR: Could not validate with device" << udid
                 << "because a passcode is set. Please enter the passcode on "
                    "the device and retry.";
        break;
    case LOCKDOWN_E_INVALID_CONF:
    case LOCKDOWN_E_INVALID_HOST_ID:
        errorMessage = QString("Device %1 is not paired with this computer.\n\n"
                               "Please check your device settings.")
                           .arg(udid);
        qDebug() << "ERROR: Device" << udid << "is not paired with this host";
        break;
    case LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING:
        errorMessage =
            QString(
                "Trust dialog is waiting for your response.\n\n"
                "Please accept the trust dialog on the screen of device %1,\n"
                "then attempt to pair again.")
                .arg(udid);
        qDebug()
            << "ERROR: Please accept the trust dialog on the screen of device"
            << udid << ", then attempt to pair again.";
        break;
    case LOCKDOWN_E_USER_DENIED_PAIRING:
        errorMessage = QString("Pairing rejected.\n\n"
                               "You denied the trust dialog on device %1.")
                           .arg(udid);
        qDebug() << "ERROR: Device" << udid
                 << "said that the user denied the trust dialog.";
        break;
    case LOCKDOWN_E_PAIRING_FAILED:
        errorMessage = QString("Pairing with device %1 failed.\n\n"
                               "Please try again or restart your device.")
                           .arg(udid);
        qDebug() << "ERROR: Pairing with device" << udid << "failed.";
        break;
    case LOCKDOWN_E_GET_PROHIBITED:
    case LOCKDOWN_E_PAIRING_PROHIBITED_OVER_THIS_CONNECTION:
        errorMessage = "Pairing is not possible over this connection.\n\n"
                       "Please try using a USB connection.";
        qDebug() << "ERROR: Pairing is not possible over this connection.";
        break;
    default:
        errorMessage = QString("Unknown error occurred with device %1.\n\n"
                               "Error code: %2")
                           .arg(udid)
                           .arg(err);
        qDebug() << "ERROR: Device" << udid << "returned unhandled error code"
                 << err;
        break;
    }
    QMessageBox errorDialog(this);
    errorDialog.setWindowTitle(errorTitle);
    errorDialog.setText(errorMessage);
    errorDialog.setIcon(QMessageBox::Warning);
    errorDialog.setStandardButtons(QMessageBox::Ok);
    errorDialog.exec();
}