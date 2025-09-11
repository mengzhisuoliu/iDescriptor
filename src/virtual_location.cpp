#include "virtual_location.h"
#include "devdiskmanager.h"
#include "iDescriptor.h"
#include <QDebug>
#include <QDoubleValidator>
#include <QGeoCoordinate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

// #include <qt6/QtPositioning/qgeocoordinate.h>

VirtualLocation::VirtualLocation(iDescriptorDevice *device, QWidget *parent)
    : QWidget{parent}, m_device(device)
{
    // Create the main layout
    bool res = DevDiskManager::sharedInstance()->mountCompatibleImage(
        m_device, QString("/tmp"));
    qDebug() << "Mount result:" << res;
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Create left panel for controls
    QWidget *rightPanel = new QWidget();
    rightPanel->setFixedWidth(250);
    rightPanel->setStyleSheet(
        "QWidget { background-color: #f0f0f0; border-radius: 5px; }");

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(15, 15, 15, 15);
    rightLayout->setSpacing(10);

    // Title
    QLabel *titleLabel = new QLabel("Virtual Location Settings");
    titleLabel->setStyleSheet(
        "font-size: 14px; font-weight: bold; color: #333;");
    rightLayout->addWidget(titleLabel);

    // Coordinates section
    QLabel *coordsLabel = new QLabel("Coordinates:");
    coordsLabel->setStyleSheet("font-weight: bold; color: #555;");
    rightLayout->addWidget(coordsLabel);

    // Latitude input
    QLabel *latLabel = new QLabel("Latitude:");
    latLabel->setStyleSheet("color: #666;");
    rightLayout->addWidget(latLabel);

    m_latitudeEdit = new QLineEdit();
    m_latitudeEdit->setPlaceholderText("e.g., 59.9139");
    m_latitudeEdit->setText("59.9139");
    m_latitudeEdit->setValidator(new QDoubleValidator(-90.0, 90.0, 6, this));
    m_latitudeEdit->setStyleSheet(
        "padding: 5px; border: 1px solid #ccc; border-radius: 3px;");
    rightLayout->addWidget(m_latitudeEdit);

    // Longitude input
    QLabel *lonLabel = new QLabel("Longitude:");
    lonLabel->setStyleSheet("color: #666;");
    rightLayout->addWidget(lonLabel);

    m_longitudeEdit = new QLineEdit();
    m_longitudeEdit->setPlaceholderText("e.g., 10.7522");
    m_longitudeEdit->setText("10.7522");
    m_longitudeEdit->setValidator(new QDoubleValidator(-180.0, 180.0, 6, this));
    m_longitudeEdit->setStyleSheet(
        "padding: 5px; border: 1px solid #ccc; border-radius: 3px;");
    rightLayout->addWidget(m_longitudeEdit);

    // Altitude input
    QLabel *altLabel = new QLabel("Altitude (meters):");
    altLabel->setStyleSheet("color: #666;");
    rightLayout->addWidget(altLabel);

    m_altitudeEdit = new QLineEdit();
    m_altitudeEdit->setPlaceholderText("e.g., 100.0");
    m_altitudeEdit->setText("100.0");
    m_altitudeEdit->setValidator(
        new QDoubleValidator(-500.0, 10000.0, 2, this));
    m_altitudeEdit->setStyleSheet(
        "padding: 5px; border: 1px solid #ccc; border-radius: 3px;");
    rightLayout->addWidget(m_altitudeEdit);

    // Add some spacing
    rightLayout->addItem(
        new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));

    // Apply button
    m_applyButton = new QPushButton("Apply Settings");
    m_applyButton->setStyleSheet("QPushButton {"
                                 "    background-color: #4CAF50;"
                                 "    color: white;"
                                 "    border: none;"
                                 "    padding: 10px;"
                                 "    border-radius: 5px;"
                                 "    font-weight: bold;"
                                 "}"
                                 "QPushButton:hover {"
                                 "    background-color: #45a049;"
                                 "}"
                                 "QPushButton:pressed {"
                                 "    background-color: #3d8b40;"
                                 "}");
    rightLayout->addWidget(m_applyButton);

    // Add stretch to push everything to the top
    rightLayout->addStretch();

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
    connect(m_altitudeEdit, &QLineEdit::textChanged, this,
            &VirtualLocation::onInputChanged);
    connect(m_applyButton, &QPushButton::clicked, this,
            &VirtualLocation::onApplyClicked);

    // Connect to QML map
    connect(m_quickWidget, &QQuickWidget::statusChanged, this,
            &VirtualLocation::onQuickWidgetStatusChanged);

    // Register this object with QML context so QML can call our slots
    m_quickWidget->rootContext()->setContextProperty("cppHandler", this);

    qDebug() << "QuickWidget status:" << m_quickWidget->status();
    qDebug() << "QuickWidget errors:" << m_quickWidget->errors();
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
    // Update map when input changes (with slight delay to avoid too frequent
    // updates)
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
    bool latOk, lonOk, altOk;
    double latitude = m_latitudeEdit->text().toDouble(&latOk);
    double longitude = m_longitudeEdit->text().toDouble(&lonOk);
    double altitude = m_altitudeEdit->text().toDouble(&altOk);

    if (latOk && lonOk && altOk) {
        // Emit signal or perform action with the coordinates
        emit locationChanged(latitude, longitude, altitude);

        // Update map one final time
        updateMapFromInputs();

        // Visual feedback
        m_applyButton->setText("Applied!");
        m_applyButton->setEnabled(false);

        QTimer::singleShot(1000, this, [this]() {
            m_applyButton->setText("Apply Settings");
            m_applyButton->setEnabled(true);
        });
        bool success = set_location(
            m_device->device,
            const_cast<char *>(m_latitudeEdit->text().toStdString().c_str()),
            const_cast<char *>(m_longitudeEdit->text().toStdString().c_str()));

        if (!success) {
            warn("Failed to set location on device");
            qDebug() << "Failed to set location on device";
        } else {
            qDebug() << "Applied location settings:" << latitude << ","
                     << longitude << "," << altitude;
        }
    } else {
        qDebug() << "Invalid coordinate values";
    }
}
