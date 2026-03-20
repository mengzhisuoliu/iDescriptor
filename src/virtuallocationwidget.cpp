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

#include "virtuallocationwidget.h"
#include "appcontext.h"
#include "devdiskimagehelper.h"
#include "devdiskmanager.h"
#include "iDescriptor.h"
#include "servicemanager.h"
#include "settingsmanager.h"
#include <QDebug>
#include <QDoubleValidator>
#include <QGeoCoordinate>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
// FIXME: on macOS setupToolFrame in Tool widget does nothing
// probably because we are using a QQuickWidget
VirtualLocation::VirtualLocation(iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_device(device)
{
    setWindowTitle("Virtual Location - iDescriptor");
    setMinimumSize(600, 400);
    resize(800, 600);

#ifdef WIN32
    setObjectName("VirtualLocationWidget");
    setAttribute(Qt::WA_StyledBackground, true);
#endif

    // Create the main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Create left panel for controls
    QWidget *rightPanel = new QWidget();
    rightPanel->setFixedWidth(250);

    m_rightLayout = new QVBoxLayout(rightPanel);
    m_rightLayout->setContentsMargins(15, 15, 15, 15);
    m_rightLayout->setSpacing(10);

    // Title
    QLabel *titleLabel = new QLabel("Virtual Location Settings");
    titleLabel->setStyleSheet("margin-bottom: 10px;");
    m_rightLayout->addWidget(titleLabel);

    QGroupBox *coordGroup = new QGroupBox("Coordinates");
    m_rightLayout->addWidget(coordGroup);

    QVBoxLayout *coordLayout = new QVBoxLayout(coordGroup);

    // Latitude input
    QLabel *latLabel = new QLabel("Latitude:");
    coordLayout->addWidget(latLabel);

    m_latitudeEdit = new QLineEdit();
    m_latitudeEdit->setPlaceholderText("e.g., 59.9139");
    m_latitudeEdit->setText("59.9139");
    m_latitudeEdit->setValidator(new QDoubleValidator(-90.0, 90.0, 6, this));
    coordLayout->addWidget(m_latitudeEdit);

    // Longitude input
    QLabel *lonLabel = new QLabel("Longitude:");
    coordLayout->addWidget(lonLabel);

    m_longitudeEdit = new QLineEdit();
    m_longitudeEdit->setPlaceholderText("e.g., 10.7522");
    m_longitudeEdit->setText("10.7522");
    m_longitudeEdit->setValidator(new QDoubleValidator(-180.0, 180.0, 6, this));
    coordLayout->addWidget(m_longitudeEdit);

    // Add some spacing
    m_rightLayout->addItem(
        new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));

    // Apply button
    m_applyButton = new QPushButton("Apply Location");
    m_applyButton->setDefault(true);
    m_rightLayout->addWidget(m_applyButton);

    // Recent locations section
    loadRecentLocations(m_rightLayout);

    // Add stretch to push everything to the top
    m_rightLayout->addStretch();

    // Connect to recent locations changes
    connect(SettingsManager::sharedInstance(),
            &SettingsManager::recentLocationsChanged, this,
            &VirtualLocation::refreshRecentLocations);

    // Create map widget
    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setSource(QUrl(QStringLiteral("qrc:/qml/MapView.qml")));

    // Enable input handling
    m_quickWidget->setFocusPolicy(Qt::StrongFocus);
    m_quickWidget->setAttribute(Qt::WA_AcceptTouchEvents, true);

    // Add widgets to main layout
    mainLayout->addWidget(m_quickWidget,
                          1); // Give map widget stretch factor of 1
    mainLayout->addWidget(rightPanel);

    setLayout(mainLayout);

    // Connect signals
    connect(m_latitudeEdit, &QLineEdit::textChanged, this,
            &VirtualLocation::onInputChanged);
    connect(m_longitudeEdit, &QLineEdit::textChanged, this,
            &VirtualLocation::onInputChanged);
    connect(m_applyButton, &QPushButton::clicked, this,
            &VirtualLocation::onApplyClicked);

    // Connect to QML map
    connect(m_quickWidget, &QQuickWidget::statusChanged, this,
            &VirtualLocation::onQuickWidgetStatusChanged);

    // Register this object with QML context so QML can call our slots
    m_quickWidget->rootContext()->setContextProperty("cppHandler", this);

    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this](const std::string &udid) {
                if (m_device->udid == udid) {
                    this->close();
                    this->deleteLater();
                }
            });
}

void VirtualLocation::onQuickWidgetStatusChanged(QQuickWidget::Status status)
{
    if (status == QQuickWidget::Ready) {
        qDebug() << "QuickWidget is ready";

        // Set initial map position
        updateMapFromInputs();
    } else if (status == QQuickWidget::Error) {
        qDebug() << "QuickWidget errors:" << m_quickWidget->errors();
    }
}

void VirtualLocation::onInputChanged()
{
    // Update map when input changes
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(500); // 500ms delay

    disconnect(&m_updateTimer, &QTimer::timeout, this,
               &VirtualLocation::updateMapFromInputs);
    connect(&m_updateTimer, &QTimer::timeout, this,
            &VirtualLocation::updateMapFromInputs);

    m_updateTimer.start();
}

void VirtualLocation::updateMapFromInputs()
{
    bool latOk, lonOk;
    double latitude = m_latitudeEdit->text().toDouble(&latOk);
    double longitude = m_longitudeEdit->text().toDouble(&lonOk);

    // FIXME: warn if not valid
    if (latOk && lonOk && latitude >= -90 && latitude <= 90 &&
        longitude >= -180 && longitude <= 180) {
        QQuickItem *rootObject = m_quickWidget->rootObject();
        if (rootObject) {
            QQuickItem *mapItem = rootObject->findChild<QQuickItem *>("map");
            if (mapItem) {
                // Block signals to prevent feedback loop
                m_updatingFromInput = true;

                // Call QML function to update map center
                QMetaObject::invokeMethod(mapItem, "updateCenter",
                                          Q_ARG(QVariant, latitude),
                                          Q_ARG(QVariant, longitude));

                m_updatingFromInput = false;

                qDebug() << "Updated map center to:" << latitude << ","
                         << longitude;
            }
        }
    }
}

void VirtualLocation::onMapCenterChanged()
{
    if (m_updatingFromInput) {
        return; // Prevent feedback loop
    }

    qDebug() << "onMapCenterChanged called!";

    QQuickItem *rootObject = m_quickWidget->rootObject();
    if (rootObject) {
        QQuickItem *mapItem = rootObject->findChild<QQuickItem *>("map");
        if (mapItem) {
            // Get map center using QMetaObject::invokeMethod for more reliable
            // access
            QVariant centerVar = mapItem->property("center");

            if (centerVar.isValid()) {
                // Try to get the coordinate directly
                QGeoCoordinate coord = centerVar.value<QGeoCoordinate>();

                if (coord.isValid()) {
                    double latitude = coord.latitude();
                    double longitude = coord.longitude();

                    // Block signals temporarily to prevent feedback
                    m_latitudeEdit->blockSignals(true);
                    m_longitudeEdit->blockSignals(true);

                    // Update input fields
                    m_latitudeEdit->setText(QString::number(latitude, 'f', 6));
                    m_longitudeEdit->setText(
                        QString::number(longitude, 'f', 6));

                    // Restore signals
                    m_latitudeEdit->blockSignals(false);
                    m_longitudeEdit->blockSignals(false);

                    qDebug() << "Updated inputs from map:" << latitude << ","
                             << longitude;
                } else {
                    qDebug() << "Invalid coordinate from map";
                }
            } else {
                qDebug() << "Could not get center property from map";
            }
        }
    }
}

// Add this new slot that QML can call directly
void VirtualLocation::updateInputsFromMap(double latitude, double longitude)
{
    if (m_updatingFromInput) {
        return; // Prevent feedback loop
    }

    qDebug() << "updateInputsFromMap called with:" << latitude << ","
             << longitude;

    // Block signals temporarily to prevent feedback
    m_latitudeEdit->blockSignals(true);
    m_longitudeEdit->blockSignals(true);

    // Update input fields
    m_latitudeEdit->setText(QString::number(latitude, 'f', 6));
    m_longitudeEdit->setText(QString::number(longitude, 'f', 6));

    // Restore signals
    m_latitudeEdit->blockSignals(false);
    m_longitudeEdit->blockSignals(false);

    qDebug() << "Updated inputs from map:" << latitude << "," << longitude;
}

void VirtualLocation::onApplyClicked()
{
    m_applyButton->setEnabled(false);
    bool latOk, lonOk;
    double latitude = m_latitudeEdit->text().toDouble(&latOk);
    double longitude = m_longitudeEdit->text().toDouble(&lonOk);

    if (!latOk || !lonOk) {
        QMessageBox::warning(
            this, "Invalid Input",
            "Please enter valid latitude and longitude values.");
        m_applyButton->setEnabled(true);
        return;
    }

    int major = m_device->deviceInfo.parsedDeviceVersion.major;

    if (major < 17) {
        LocationSimulationServiceHandle *locationSim = nullptr;
        IdeviceFfiError *err = nullptr;
        err = lockdown_location_simulation_connect(m_device->provider,
                                                   &locationSim);
        if (err != NULL) {
            qDebug() << "Failed to connect to location simulation service:"
                     << err->code << err->message;
            if (err->code != InvalidServiceErrorCode) {
                QMessageBox::warning(
                    this, "Error",
                    QString(
                        "Failed to connect to location simulation service: %1")
                        .arg(err->message));
                idevice_error_free(err);
                m_applyButton->setEnabled(true);
                return;
            }

            idevice_error_free(err);
            DevDiskImageHelper *devDiskImageHelper =
                new DevDiskImageHelper(m_device, this);
            connect(
                devDiskImageHelper, &DevDiskImageHelper::mountingCompleted,
                this,
                [this, latitude, longitude, devDiskImageHelper](bool success) {
                    if (devDiskImageHelper) {
                        devDiskImageHelper->deleteLater();
                    }

                    if (!success) {
                        // mounter will show its own error message, just
                        // re-enable the button here
                        m_applyButton->setEnabled(true);
                        return;
                    }
                    IdeviceFfiError *err = nullptr;
                    LocationSimulationServiceHandle *locationSim = nullptr;
                    err = lockdown_location_simulation_connect(
                        m_device->provider, &locationSim);

                    if (err != NULL) {
                        qDebug() << "Failed to connect to location simulation "
                                    "service after mounting disk image:"
                                 << err->code << err->message;
                        QMessageBox::warning(
                            this, "Error",
                            QString("Failed to connect to location simulation "
                                    "service after mounting disk image: %1")
                                .arg(err->message));
                        idevice_error_free(err);
                        m_applyButton->setEnabled(true);
                        return;
                    }

                    err = lockdown_location_simulation_set(
                        locationSim,
                        m_latitudeEdit->text().toStdString().c_str(),
                        m_longitudeEdit->text().toStdString().c_str());
                    if (err != NULL) {
                        qDebug()
                            << "Failed to set location simulation:" << err->code
                            << err->message;
                        QMessageBox::warning(
                            this, "Error",
                            QString("Failed to set location simulation: %1")
                                .arg(err->message));
                        idevice_error_free(err);
                    }

                    lockdown_location_simulation_free(locationSim);
                    QMessageBox::information(this, "Success",
                                             "Location applied successfully!");

                    SettingsManager::sharedInstance()->saveRecentLocation(
                        latitude, longitude);
                    m_applyButton->setEnabled(true);
                    return;
                });
            connect(
                devDiskImageHelper, &DevDiskImageHelper::destroyed, this,
                [this]() {
                    QTimer::singleShot(1000, this, [this]() {
                        m_applyButton->setText("Apply Location");
                        m_applyButton->setEnabled(true);
                    });
                },
                Qt::SingleShotConnection);
            return devDiskImageHelper->start();
        }

        err = lockdown_location_simulation_set(
            locationSim, m_latitudeEdit->text().toStdString().c_str(),
            m_longitudeEdit->text().toStdString().c_str());
        if (err != NULL) {
            qDebug() << "Failed to set location simulation:" << err->code
                     << err->message;
            QMessageBox::warning(
                this, "Error",
                QString("Failed to set location simulation: %1")
                    .arg(err->message));
            idevice_error_free(err);
        }

        lockdown_location_simulation_free(locationSim);
        QMessageBox::information(this, "Success",
                                 "Location applied successfully!");
        m_applyButton->setEnabled(true);
        return;
    }

    /* iOS 17 and above */
    auto tmpProvider = IdeviceFFI::Provider::adopt(m_device->provider);

    auto cdp_res = IdeviceFFI::CoreDeviceProxy::connect(tmpProvider);

    // provider shouldn't be freed
    tmpProvider.release();

    if (cdp_res.is_err()) {
        auto err = cdp_res.unwrap_err();
        qDebug() << "Failed to connect to CoreDeviceProxy:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(this, "Error",
                             QString("Failed to connect to CoreDeviceProxy: %1")
                                 .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    auto &cdp = cdp_res.unwrap();

    auto rsd_port_res = cdp.get_server_rsd_port();
    if (rsd_port_res.is_err()) {
        auto err = rsd_port_res.unwrap_err();
        qDebug() << "Failed to get server RSD port:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(this, "Error",
                             QString("Failed to get server RSD port: %1")
                                 .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    uint16_t rsd_port = rsd_port_res.unwrap();

    auto adapter_res = std::move(cdp).create_tcp_adapter();
    if (adapter_res.is_err()) {
        auto err = adapter_res.unwrap_err();
        qDebug() << "Failed to create software tunnel adapter:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(
            this, "Error",
            QString("Failed to create software tunnel adapter: %1")
                .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    auto &adapter = adapter_res.unwrap();

    auto stream_res = adapter.connect(rsd_port);
    if (stream_res.is_err()) {
        auto err = stream_res.unwrap_err();
        qDebug() << "Failed to connect RSD stream:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(this, "Error",
                             QString("Failed to connect RSD stream: %1")
                                 .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    auto &stream = stream_res.unwrap();

    auto rsd_res = IdeviceFFI::RsdHandshake::from_socket(std::move(stream));
    if (rsd_res.is_err()) {
        auto err = rsd_res.unwrap_err();
        qDebug() << "Failed RSD handshake:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(this, "Error",
                             QString("Failed RSD handshake: %1")
                                 .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    auto &rsd = rsd_res.unwrap();

    auto rs_res = IdeviceFFI::RemoteServer::connect_rsd(adapter, rsd);
    if (rs_res.is_err()) {
        auto err = rs_res.unwrap_err();
        qDebug() << "Failed to connect to RemoteServer:" << err.code
                 << QString::fromStdString(err.message);

        IdeviceFfiError *err_reveal = nullptr;
        if (err.code == ServiceNotFoundErrorCode) {
            err_reveal =
                ServiceManager::revealDeveloperModeOptionInUI(m_device);
            if (err_reveal != NULL) {
                qDebug() << "Failed to reveal developer mode option in UI:"
                         << err_reveal->code << err_reveal->message;
                QMessageBox::warning(
                    this, "Error",
                    "Developer Mode Not Enabled and failed to "
                    "reveal developer mode option in UI:\n" +
                        QString::fromStdString(err_reveal->message));
                idevice_error_free(err_reveal);
            }
            // TODO: create a widget showing instructions on how to enable dev
            // mode,
            QMessageBox::information(this, "Developer Mode Not Enabled",
                                     "Please enable Developer "
                                     "Mode on the device to use this "
                                     "feature.");

            m_applyButton->setEnabled(true);
            return;
        }

        QMessageBox::warning(this, "Error",
                             QString("Failed to connect to RemoteServer: %1")
                                 .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    auto &rs = rs_res.unwrap();

    auto sim_res = IdeviceFFI::LocationSimulation::create(rs);
    if (sim_res.is_err()) {
        auto err = sim_res.unwrap_err();
        qDebug() << "Failed to create LocationSimulation client:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(
            this, "Error",
            QString("Failed to create LocationSimulation client: %1")
                .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    auto &sim = sim_res.unwrap();

    auto set_res = sim.set(latitude, longitude);
    if (set_res.is_err()) {
        auto err = set_res.unwrap_err();
        qDebug() << "Failed to set location simulation:" << err.code
                 << QString::fromStdString(err.message);
        QMessageBox::warning(this, "Error",
                             QString("Failed to set location simulation: %1")
                                 .arg(QString::fromStdString(err.message)));
        m_applyButton->setEnabled(true);
        return;
    }

    SettingsManager::sharedInstance()->saveRecentLocation(latitude, longitude);
    m_applyButton->setEnabled(true);
    QMessageBox::information(this, "Success", "Location applied successfully");
}

void VirtualLocation::loadRecentLocations(QVBoxLayout *layout)
{
    QList<QVariantMap> recentLocations =
        SettingsManager::sharedInstance()->getRecentLocations();

    if (recentLocations.isEmpty()) {
        return; // Don't render anything if no recent locations
    }

    layout->addItem(
        new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));

    m_recentGroup = new QGroupBox("Recent Locations");
    layout->addWidget(m_recentGroup);

    // A group box needs a layout to contain its children
    QVBoxLayout *groupBoxLayout = new QVBoxLayout(m_recentGroup);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    groupBoxLayout->addWidget(scrollArea);

    QWidget *scrollContent = new QWidget();
    scrollArea->setWidget(scrollContent);

    // This layout is for the content widget
    QVBoxLayout *recentLayout = new QVBoxLayout(scrollContent);

    addLocationButtons(recentLayout, recentLocations);
}

void VirtualLocation::onRecentLocationClicked(double latitude, double longitude)
{
    // Update input fields
    m_latitudeEdit->setText(QString::number(latitude, 'f', 6));
    m_longitudeEdit->setText(QString::number(longitude, 'f', 6));

    // Update map
    updateMapFromInputs();

    qDebug() << "Recent location clicked:" << latitude << "," << longitude;
}

void VirtualLocation::refreshRecentLocations()
{
    if (!m_recentGroup) {
        return;
    }

    // Get the group box's layout
    QVBoxLayout *groupBoxLayout =
        qobject_cast<QVBoxLayout *>(m_recentGroup->layout());
    if (!groupBoxLayout) {
        return;
    }

    // Get the scroll area from the group box layout
    QScrollArea *scrollArea = nullptr;
    if (groupBoxLayout->count() > 0) {
        scrollArea =
            qobject_cast<QScrollArea *>(groupBoxLayout->itemAt(0)->widget());
    }

    if (!scrollArea) {
        return;
    }

    // Get the scroll content widget
    QWidget *scrollContent = scrollArea->widget();
    if (!scrollContent) {
        return;
    }

    // Get the content layout
    QVBoxLayout *recentLayout =
        qobject_cast<QVBoxLayout *>(scrollContent->layout());
    if (!recentLayout) {
        return;
    }

    // Clear all existing buttons
    QLayoutItem *item;
    while ((item = recentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Reload recent locations
    QList<QVariantMap> recentLocations =
        SettingsManager::sharedInstance()->getRecentLocations();

    if (recentLocations.isEmpty()) {
        // Hide the group if no locations
        m_recentGroup->hide();
        return;
    }

    // Show the group if it was hidden
    m_recentGroup->show();

    addLocationButtons(recentLayout, recentLocations);
}

void VirtualLocation::addLocationButtons(QLayout *layout,
                                         QList<QVariantMap> recentLocations)
{
    for (const QVariantMap &location : recentLocations) {
        double latitude = location["latitude"].toDouble();
        double longitude = location["longitude"].toDouble();

        QPushButton *locationBtn =
            new QPushButton(QString("Lat: %1\nLon: %2")
                                .arg(latitude, 0, 'f', 4)
                                .arg(longitude, 0, 'f', 4));

        connect(locationBtn, &QPushButton::clicked, this,
                [this, latitude, longitude]() {
                    onRecentLocationClicked(latitude, longitude);
                });

        layout->addWidget(locationBtn);
    }
}
