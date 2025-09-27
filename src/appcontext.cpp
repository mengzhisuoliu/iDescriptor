#include "appcontext.h"
#include "iDescriptor.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QMessageBox>
#include <QUuid>

AppContext *AppContext::sharedInstance()
{
    static AppContext instance;
    return &instance;
}

AppContext::AppContext(QObject *parent) : QObject{parent}
{
    // TODO: IMPLEMENT
    // QDBusConnection bus = QDBusConnection::systemBus();

    // // Connect to the logind Manager's PrepareForSleep signal
    // bool connected = bus.connect("org.freedesktop.login1",  // service
    //                              "/org/freedesktop/login1", // object path
    //                              "org.freedesktop.login1.Manager", //
    //                              interface "PrepareForSleep", // signal name
    //                              this,              // receiver
    //                              SLOT(handleDBusSignal(QDBusMessage &)) //
    //                              slot
    // );

    // if (!connected) {
    //     qDebug() << "Failed to connect to PrepareForSleep signal.";
    // } else {
    //     qDebug() << "Successfully connected to PrepareForSleep signal.";
    // }
}

void AppContext::handleDBusSignal(const QDBusMessage &msg)
{
    if (msg.arguments().isEmpty()) {
        qWarning() << "Received PrepareForSleep signal with no arguments.";
        return;
    }

    QVariant firstArg = msg.arguments().at(0);
    if (!firstArg.canConvert<bool>()) {
        qWarning()
            << "First argument of PrepareForSleep signal is not a boolean.";
        return;
    }

    bool isSleeping = firstArg.toBool();
    if (isSleeping) {
        qDebug() << "System is going to sleep...";
        emit systemSleepStarting();
        // Clean up device resources before sleep
        // cleanupAllDevices();
    } else {
        qDebug() << "System is resuming from sleep.";
        emit systemWakeup();
    }
}

void AppContext::addDevice(QString udid, idevice_connection_type conn_type,
                           AddType addType)
{
    try {
        IDescriptorInitDeviceResult initResult =
            init_idescriptor_device(udid.toStdString().c_str());

        qDebug() << "init_idescriptor_device success ?: " << initResult.success;
        qDebug() << "init_idescriptor_device error code: " << initResult.error;

        if (!initResult.success) {
            qDebug() << "Failed to initialize device with UDID: " << udid;
            // return onDeviceInitFailed(udid, initResult.error);
            if (initResult.error == LOCKDOWN_E_PASSWORD_PROTECTED) {
                // warn("Device with UDID " + udid +
                //          " is password protected. Please unlock the device
                //          and " "try again.",
                //      "Warning");
                // TODO: also handle pairing devices
                // the reason why we don't handle pairing devices here is
                // because it's less likely that it will be an error typeof
                // LOCKDOWN_E_PASSWORD_PROTECTED if the device is paired type
                if (addType == AddType::Regular) { //  TODO:IMPLEMENT
                    // emit devicePasswordProtected(udid);
                };
            } else if (initResult.error ==
                       LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING) {
                m_pendingDevices.append(udid);
                // FIXME: if a device never gets paired, it will stay in this
                // list forever
                emit devicePairPending(udid);

                // warn("Device with UDID " + udid +
                //          " is not trusted. Please trust this computer on the
                //          " "device and try again.",
                //      "Warning");
            } else {
                warn("Failed to initialize device with UDID " + udid +
                         ". Error code: " + QString::number(initResult.error),
                     "Warning");
            }
            return;
        }
        qDebug() << "Device initialized: " << udid;

        iDescriptorDevice *device = new iDescriptorDevice{
            .udid = udid.toStdString(),
            .conn_type = conn_type,
            .device = initResult.device,
            .deviceInfo = initResult.deviceInfo,
            .afcClient = initResult.afcClient,
            .afc2Client = initResult.afc2Client,
        };
        m_devices[device->udid] = device;
        if (addType == AddType::Regular)
            return emit deviceAdded(device);
        emit devicePaired(device);
        m_pendingDevices.removeAll(udid);

    } catch (const std::exception &e) {
        qDebug() << "Exception in onDeviceAdded: " << e.what();
        // QMessageBox::critical(this, "Error", "An error occurred while
        // processing device information");
    }
}
// TODO:WIP
void AppContext::instanceRemoveDevice(QString _udid)
{
    const std::string uuid = _udid.toStdString();
    if (!m_devices.contains(uuid)) {
        qDebug() << "Device with UUID " + _udid +
                        " not found. Please report this issue.",
            "Error";
        return;
    }

    qDebug() << "Removing device with UUID:" << QString::fromStdString(uuid);

    // cleanDevice(device);
    iDescriptorDevice *device = m_devices[uuid];
    m_devices.remove(uuid);

    emit deviceRemoved(uuid);
    // TODO: Cleanup now should be done wherever there are initialized
    //  lockdownd_client_free(device->lockdownClient);
    if (device->afcClient)
        afc_client_free(device->afcClient);
    idevice_free(device->device);
    // lockdownd_service_descriptor_free(device->lockdownService);
    delete device;
    // return true;
}

void AppContext::removeDevice(QString _udid)

{
    const std::string uuid = _udid.toStdString();
    if (!m_devices.contains(uuid)) {
        qDebug() << "Device with UUID " + _udid +
                        " not found. Please report this issue.",
            "Error";
        return;
    }

    qDebug() << "Removing device with UUID:" << QString::fromStdString(uuid);

    // cleanDevice(device);
    iDescriptorDevice *device = m_devices[uuid];
    m_devices.remove(uuid);

    emit deviceRemoved(uuid);
    // TODO: Cleanup now should be done wherever there are initialized
    //  lockdownd_client_free(device->lockdownClient);
    if (device->afcClient)
        afc_client_free(device->afcClient);
    idevice_free(device->device);
    // lockdownd_service_descriptor_free(device->lockdownService);
    delete device;
    // return true;
}

void AppContext::removeRecoveryDevice(const QString &ecid)
{
    std::string std_ecid = ecid.toStdString();
    if (!m_recoveryDevices.contains(std_ecid)) {
        // warning popup
        warn("Device with ECID " + ecid +
                 " not found. Please report this issue.",
             "Error");
        return;
    }

    qDebug() << "Removing recovery device with ECID:"
             << QString::fromStdString(std_ecid);

    RecoveryDeviceInfo *deviceInfo = m_recoveryDevices[std_ecid];
    m_recoveryDevices.remove(std_ecid);

    delete deviceInfo;

    emit recoveryDeviceRemoved(ecid);
}

iDescriptorDevice *AppContext::getDevice(const std::string &uuid)
{
    return m_devices.value(uuid, nullptr);
}

QList<iDescriptorDevice *> AppContext::getAllDevices()
{
    return m_devices.values();
}

QList<RecoveryDeviceInfo *> AppContext::getAllRecoveryDevices()
{
    return m_recoveryDevices.values();
}

// Returns whether there are any devices connected (regular or recovery)
bool AppContext::noDevicesConnected() const
{
    return (m_devices.isEmpty() && m_recoveryDevices.isEmpty() &&
            m_pendingDevices.isEmpty());
}

std::string AppContext::addRecoveryDevice(RecoveryDeviceInfo *deviceInfo)
{
    // Generate a unique ID for the recovery device
    // std::string uuid =
    // QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    // Add the device to the map
    // uint64_t to std::string
    m_recoveryDevices[std::to_string(deviceInfo->ecid)] = deviceInfo;

    // emit recoveryDeviceAdded(uuid);
    return std::to_string(deviceInfo->ecid);
}

AppContext::~AppContext()
{
    // Cleanup if needed
}