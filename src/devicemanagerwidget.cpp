#include "devicemanagerwidget.h"
#include "appcontext.h"
#include "devicemenuwidget.h"
#include "devicependingwidget.h"
#include "recoverydeviceinfowidget.h"
#include "settingsmanager.h"
#include <QDebug>

DeviceManagerWidget::DeviceManagerWidget(QWidget *parent)
    : QWidget(parent), m_currentDeviceUuid("")
{
    setupUI();

    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            [this](iDescriptorDevice *device) {
                addDevice(device);

                // Apply settings-based behavior for switching to new device
                SettingsManager::sharedInstance()->doIfEnabled(
                    SettingsManager::Setting::SwitchToNewDevice,
                    [this, device]() {
                        AppContext::sharedInstance()->setCurrentDeviceSelection(
                            DeviceSelection(device->udid));
                    });

                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this](const std::string &uuid) {
                removeDevice(uuid);
                auto devices = AppContext::sharedInstance()->getAllDevices();
                if (!devices.isEmpty())
                    AppContext::sharedInstance()->setCurrentDeviceSelection(
                        DeviceSelection(devices.first()->udid));
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::devicePairPending, this,
            [this](const QString &udid) {
                addPendingDevice(udid, false);
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::devicePasswordProtected,
            this, [this](const QString &udid) {
                addPendingDevice(udid, true);
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::devicePaired, this,
            [this](iDescriptorDevice *device) {
                addPairedDevice(device);
                SettingsManager::sharedInstance()->doIfEnabled(
                    SettingsManager::Setting::SwitchToNewDevice,
                    [this, device]() {
                        AppContext::sharedInstance()->setCurrentDeviceSelection(
                            DeviceSelection(device->udid));
                    });

                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::recoveryDeviceAdded,
            this, [this](const iDescriptorRecoveryDevice *recoveryDeviceInfo) {
                addRecoveryDevice(recoveryDeviceInfo);
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::recoveryDeviceRemoved,
            this, [this](uint64_t ecid) {
                removeRecoveryDevice(ecid);
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::devicePairingExpired,
            this, [this](const QString &udid) {
                removePendingDevice(udid);
                emit updateNoDevicesConnected();
            });
    onDeviceSelectionChanged(
        AppContext::sharedInstance()->getCurrentDeviceSelection());
}

void DeviceManagerWidget::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create sidebar
    m_sidebar = new DeviceSidebarWidget();

    // Create stacked widget for device content
    m_stackedWidget = new QStackedWidget();

    // Add to layout
    m_mainLayout->addWidget(m_sidebar);
    m_mainLayout->addWidget(m_stackedWidget);

    // Connect signals
    connect(AppContext::sharedInstance(),
            &AppContext::currentDeviceSelectionChanged, this,
            &DeviceManagerWidget::onDeviceSelectionChanged);
}

void DeviceManagerWidget::addDevice(iDescriptorDevice *device)
{
    if (m_deviceWidgets.contains(device->udid)) {
        qWarning() << "Device already exists:"
                   << QString::fromStdString(device->udid);
        return;
    }
    qDebug() << "Connect ::deviceAdded Adding:"
             << QString::fromStdString(device->udid);

    DeviceMenuWidget *deviceWidget = new DeviceMenuWidget(device, this);

    QString tabTitle = QString::fromStdString(device->deviceInfo.productType);

    m_stackedWidget->addWidget(deviceWidget);
    m_deviceWidgets[device->udid] =
        std::pair{deviceWidget, m_sidebar->addDevice(tabTitle, device->udid)};
}

void DeviceManagerWidget::addRecoveryDevice(
    const iDescriptorRecoveryDevice *device)
{
    try {
        // Create device info widget
        RecoveryDeviceInfoWidget *recoveryDeviceInfoWidget =
            new RecoveryDeviceInfoWidget(device);
        m_recoveryDeviceWidgets.insert(
            device->ecid,
            std::pair{recoveryDeviceInfoWidget,
                      m_sidebar->addRecoveryDevice(device->ecid)});
        m_stackedWidget->addWidget(recoveryDeviceInfoWidget);

    } catch (...) {
        qDebug() << "Error initializing recovery device";
    }
}

void DeviceManagerWidget::removeRecoveryDevice(uint64_t ecid)
{
    qDebug() << "Removing recovery device with ECID:" << ecid;
    if (!m_recoveryDeviceWidgets.contains(ecid)) {
        qDebug() << "Recovery device with ECID" + QString::number(ecid) +
                        " not found. Please report this issue.";
        return;
    }

    RecoveryDeviceInfoWidget *deviceWidget =
        m_recoveryDeviceWidgets[ecid].first;
    RecoveryDeviceSidebarItem *sidebarItem =
        m_recoveryDeviceWidgets[ecid].second;

    if (deviceWidget != nullptr && sidebarItem != nullptr) {
        qDebug() << "Recovery device exists removing:" << QString::number(ecid);

        m_recoveryDeviceWidgets.remove(ecid);
        m_stackedWidget->removeWidget(deviceWidget);
        m_sidebar->removeRecoveryDevice(ecid);
        deviceWidget->deleteLater();

        emit updateNoDevicesConnected();
    }
}

void DeviceManagerWidget::addPendingDevice(const QString &udid, bool locked)
{
    qDebug() << "Adding pending device:" << udid;
    if (m_pendingDeviceWidgets.contains(udid.toStdString()) && !locked) {
        qDebug() << "Pending device already exists, moving to next state:"
                 << udid;
        m_pendingDeviceWidgets[udid.toStdString()].first->next();
        return;
    } else if (m_pendingDeviceWidgets.contains(udid.toStdString()) && locked) {
        // Already exists and still locked, do nothing
        qDebug()
            << "Pending device already exists and is locked, doing nothing:"
            << udid;
        return;
    }

    qDebug() << "Created pending widget for:" << udid << "Locked:" << locked;
    DevicePendingWidget *pendingWidget = new DevicePendingWidget(locked, this);
    m_stackedWidget->addWidget(pendingWidget);
    m_pendingDeviceWidgets[udid.toStdString()] =
        std::pair{pendingWidget, m_sidebar->addPendingDevice(udid)};
}

void DeviceManagerWidget::removePendingDevice(const QString &udid)
{
    qDebug() << "Removing pending device:" << udid;
    if (!m_pendingDeviceWidgets.contains(udid.toStdString())) {
        qDebug() << "Pending device not found:" << udid;
        return;
    }
    std::string udidStr = udid.toStdString();
    DevicePendingWidget *deviceWidget = m_pendingDeviceWidgets[udidStr].first;
    DevicePendingSidebarItem *sidebarItem =
        m_pendingDeviceWidgets[udidStr].second;

    if (deviceWidget != nullptr && sidebarItem != nullptr) {
        qDebug() << "Pending device exists removing:" << udid;
        m_pendingDeviceWidgets.remove(udidStr);
        m_stackedWidget->removeWidget(deviceWidget);
        m_sidebar->removePendingDevice(udidStr);
        deviceWidget->deleteLater();
    }
}

void DeviceManagerWidget::addPairedDevice(iDescriptorDevice *device)
{
    qDebug() << "Device paired:" << QString::fromStdString(device->udid);

    // Check if pending device exists
    if (m_pendingDeviceWidgets.contains(device->udid)) {
        std::pair<DevicePendingWidget *, DevicePendingSidebarItem *> &pair =
            m_pendingDeviceWidgets[device->udid];

        // Remove from sidebar if it exists
        if (pair.second) {
            qDebug() << "Removing pending device from sidebar:"
                     << QString::fromStdString(device->udid);
            m_sidebar->removePendingDevice(device->udid);
        }

        // Clean up widget if it exists
        if (pair.first) {
            qDebug() << "Removing pending device widget:"
                     << QString::fromStdString(device->udid);
            m_stackedWidget->removeWidget(pair.first);
            pair.first->deleteLater();
        }

        m_pendingDeviceWidgets.remove(device->udid);
    }

    addDevice(device);
}

void DeviceManagerWidget::removeDevice(const std::string &uuid)
{

    qDebug() << "Removing:" << QString::fromStdString(uuid);
    DeviceMenuWidget *deviceWidget = m_deviceWidgets[uuid].first;
    DeviceSidebarItem *sidebarItem = m_deviceWidgets[uuid].second;

    if (deviceWidget != nullptr && sidebarItem != nullptr) {
        qDebug() << "Device exists removing:" << QString::fromStdString(uuid);
        // TODO: cleanups
        m_deviceWidgets.remove(uuid);
        m_stackedWidget->removeWidget(deviceWidget);
        m_sidebar->removeDevice(uuid);
        deviceWidget->deleteLater();

        // // TODO:
        // if (m_deviceWidgets.count() > 0) {
        //     setCurrentDevice(m_deviceWidgets.firstKey());
        // }
    }
}

void DeviceManagerWidget::setCurrentDevice(const std::string &uuid)
{
    qDebug() << "Setting current device to:" << QString::fromStdString(uuid);
    if (m_currentDeviceUuid == uuid)
        return;

    if (!m_deviceWidgets.contains(uuid)) {
        qWarning() << "Device UUID not found:" << QString::fromStdString(uuid);
        return;
    }

    m_currentDeviceUuid = uuid;

    QWidget *widget = m_deviceWidgets[uuid].first;
    m_stackedWidget->setCurrentWidget(widget);

    // This creates a feedback loop. The widget should only react to state
    // changes from AppContext, not create them here.
    // AppContext::sharedInstance()->setCurrentDeviceSelection(
    //     DeviceSelection(uuid));
}

std::string DeviceManagerWidget::getCurrentDevice() const
{
    return m_currentDeviceUuid;
}

void DeviceManagerWidget::onDeviceSelectionChanged(
    const DeviceSelection &selection)
{
    // Update sidebar selection
    m_sidebar->setCurrentSelection(selection);

    switch (selection.type) {
    case DeviceSelection::Normal:
        if (m_deviceWidgets.contains(selection.uuid)) {
            if (m_currentDeviceUuid != selection.uuid) {
                setCurrentDevice(selection.uuid);
            }

            // Handle navigation section
            QWidget *tabWidget = m_deviceWidgets[selection.uuid].first;
            DeviceMenuWidget *deviceMenuWidget =
                qobject_cast<DeviceMenuWidget *>(tabWidget);
            qDebug() << "Switching to tab:" << selection.section
                     << deviceMenuWidget;
            if (deviceMenuWidget && !selection.section.isEmpty()) {
                deviceMenuWidget->switchToTab(selection.section);
            }
        }
        break;

    case DeviceSelection::Recovery:
        if (m_recoveryDeviceWidgets.contains(selection.ecid)) {
            QWidget *tabWidget = m_recoveryDeviceWidgets[selection.ecid].first;
            if (tabWidget) {
                m_stackedWidget->setCurrentWidget(tabWidget);
                // Clear current device since we're viewing recovery device
                m_currentDeviceUuid = "";
            }
        }
        break;

    case DeviceSelection::Pending:
        if (m_pendingDeviceWidgets.contains(selection.uuid)) {
            QWidget *tabWidget = m_pendingDeviceWidgets[selection.uuid].first;
            if (tabWidget) {
                m_stackedWidget->setCurrentWidget(tabWidget);
                // Clear current device since we're viewing pending device
                m_currentDeviceUuid = "";
            }
        }
        break;
    }
}