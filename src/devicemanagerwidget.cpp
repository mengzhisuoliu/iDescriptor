#include "devicemanagerwidget.h"
#include "appcontext.h"
#include "devicemenuwidget.h"
#include "devicependingwidget.h"
#include <QDebug>

DeviceManagerWidget::DeviceManagerWidget(QWidget *parent)
    : QWidget(parent), m_currentDeviceUuid("")
{
    setupUI();

    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            [this](iDescriptorDevice *device) {
                addDevice(device);
                setCurrentDevice(device->udid);
                emit updateNoDevicesConnected();
            });

    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this](const std::string &uuid) {
                removeDevice(uuid);
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
                emit updateNoDevicesConnected();
            });

    // connect(AppContext::sharedInstance(), &AppContext::recoveryDeviceRemoved,
    //         this, [this](const QString &ecid) {
    //             qDebug() << "Removing:" << ecid;
    //             std::string ecidStr = ecid.toStdString();
    //             DeviceMenuWidget *deviceWidget =
    //                 qobject_cast<DeviceMenuWidget *>(
    //                     m_device_menu_widgets[ecidStr]);

    //             if (deviceWidget) {
    //                 // TODO: Implement proper removal by device index
    //                 m_device_menu_widgets.erase(ecidStr);
    //                 delete deviceWidget;
    //             }
    //             emit updateNoDevicesConnected();
    //         });
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
    connect(m_sidebar, &DeviceSidebarWidget::sidebarDeviceChanged, this,
            &DeviceManagerWidget::onSidebarDeviceChanged);
    connect(m_sidebar, &DeviceSidebarWidget::sidebarNavigationChanged, this,
            &DeviceManagerWidget::onSidebarNavigationChanged);
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
    m_deviceWidgets[device->udid] = std::pair{
        deviceWidget, m_sidebar->addToSidebar(tabTitle, device->udid)};

    // If this is the first device, make it current
    // if (m_currentDeviceIndex == -1) {
    //     setCurrentDevice(deviceIndex);
    // }
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
        std::pair{pendingWidget, m_sidebar->addPendingToSidebar(udid)};

    // If this is the first device, make it current
    // if (m_currentDeviceIndex == -1) {
    //     setCurrentDevice(deviceIndex);
    // }
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
            m_sidebar->removePendingFromSidebar(pair.second);
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
        m_sidebar->removeFromSidebar(sidebarItem);
        deviceWidget->deleteLater();
        // delete d.second;

        if (m_deviceWidgets.count() > 0) {
            setCurrentDevice(m_deviceWidgets.firstKey());
            m_sidebar->updateSidebar(m_deviceWidgets.firstKey());
        }
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

    // m_currentDeviceIndex = deviceIndex;
    m_currentDeviceUuid = uuid;

    // // Update sidebar selection
    // m_sidebar->setCurrentDevice(deviceIndex);

    // // Update stacked widget
    QWidget *widget = m_deviceWidgets[uuid].first;
    m_stackedWidget->setCurrentWidget(widget);

    emit deviceChanged(uuid);
}

std::string DeviceManagerWidget::getCurrentDevice() const
{
    return m_currentDeviceUuid;
}

QWidget *DeviceManagerWidget::getDeviceWidget(int deviceIndex) const
{
    // return m_deviceWidgets.value(deviceIndex, nullptr);
}

void DeviceManagerWidget::setDeviceNavigation(int deviceIndex,
                                              const QString &section)
{
    m_sidebar->setDeviceNavigationSection(deviceIndex, section);
    // emit deviceNavigationChanged(deviceIndex, section);
}

void DeviceManagerWidget::onSidebarDeviceChanged(std::string deviceUuid)
{
    setCurrentDevice(deviceUuid);
}

void DeviceManagerWidget::onSidebarNavigationChanged(std::string deviceUuid,
                                                     const QString &section)
{
    if (deviceUuid != m_currentDeviceUuid) {
        setCurrentDevice(deviceUuid);
    }

    QWidget *tabWidget = m_deviceWidgets[deviceUuid].first;
    DeviceMenuWidget *deviceMenuWidget =
        qobject_cast<DeviceMenuWidget *>(tabWidget);

    if (deviceMenuWidget) {
        // Call a method to change the internal tab
        deviceMenuWidget->switchToTab(section);
    }
    // if (deviceIndex != m_currentDeviceIndex) {
    //     setCurrentDevice(deviceIndex);
    // }
    // emit sidebarNavigationChanged(deviceUuid, section);
}
