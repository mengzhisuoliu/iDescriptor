#include "diskusagewidget.h"
#include "diskusagebar.h"
#include "iDescriptor.h"

#include <QApplication>
#include <QDebug>
#include <QFutureWatcher>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrent>

#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

DiskUsageWidget::DiskUsageWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_device(device), m_state(Loading), m_totalCapacity(0),
      m_systemUsage(0), m_appsUsage(0), m_mediaUsage(0), m_othersUsage(0),
      m_freeSpace(0)
{
    setMinimumHeight(80);
    setupUI();
    fetchData();
}

void DiskUsageWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 14, 10);
    m_mainLayout->setSpacing(0);

    // Title
    m_titleLabel = new QLabel("Disk Usage", this);
    QFont titleFont = font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_titleLabel);

    // Status label (for loading/error states)
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setText("Loading disk usage...");
    m_mainLayout->addWidget(m_statusLabel);

    // Disk usage bar container
    m_diskBarContainer = new QWidget(this);
    m_diskBarContainer->setMinimumHeight(20);
    m_diskBarContainer->setMaximumHeight(20);
    m_diskBarContainer->setStyleSheet(
        "QWidget#diskBarContainer { margin: 0; padding: 0; border: none; }");
    m_diskBarContainer->setObjectName("diskBarContainer");
    m_diskBarLayout = new QHBoxLayout(m_diskBarContainer);
    m_diskBarLayout->setContentsMargins(0, 0, 0, 0);
    m_diskBarLayout->setSpacing(0);

// Create colored segments
#ifdef Q_OS_MAC
    m_systemBar = new DiskUsageBar();
    m_appsBar = new DiskUsageBar();
    m_mediaBar = new DiskUsageBar();
    m_othersBar = new DiskUsageBar();
    m_freeBar = new DiskUsageBar();
#else
    m_systemBar = new QWidget();
    m_appsBar = new QWidget();
    m_mediaBar = new QWidget();
    m_othersBar = new QWidget();
    m_freeBar = new QWidget();
#endif
    // Set colors
    m_systemBar->setStyleSheet(
        "background-color: #a1384d; border: 1px solid"
        "#e64a5b; padding: 0; margin: 0; border-top-left-radius: 3px; "
        "border-bottom-left-radius: 3px;");
    m_appsBar->setStyleSheet("background-color: #4f869f; border: 1px solid "
                             "#63b4da; padding: 0; margin: 0;");
    m_mediaBar->setStyleSheet(
        "background-color: #2ECC71; border: none; padding: 0; margin: 0;");
    m_othersBar->setStyleSheet("background-color: #a28729; border: 1px solid "
                               "#c4a32d; padding: 0; margin: 0;");
    m_freeBar->setStyleSheet(
        "background-color: #474747; border: 1px solid "
        "#4f4f4f; padding: 0; margin: 0; border-top-right-radius: 3px; "
        "border-bottom-right-radius: 3px;");

    m_diskBarLayout->addWidget(m_systemBar);
    m_diskBarLayout->addWidget(m_appsBar);
    m_diskBarLayout->addWidget(m_mediaBar);
    m_diskBarLayout->addWidget(m_othersBar);
    m_diskBarLayout->addWidget(m_freeBar);

    m_diskBarContainer->hide(); // Initially hidden
    m_mainLayout->addWidget(m_diskBarContainer);

    // Legend layout
    m_legendLayout = new QHBoxLayout();
    m_legendLayout->setSpacing(0);
    m_legendLayout->setContentsMargins(0, 0, 0, 0);

    // Legend labels
    m_systemLabel = new QLabel("System", this);
    m_appsLabel = new QLabel("Apps", this);
    m_mediaLabel = new QLabel("Media", this);
    m_othersLabel = new QLabel("Others", this);
    m_freeLabel = new QLabel("Free", this);

    // Style legend labels with colored backgrounds
    QString labelStyle =
        "padding: 2px 6px; margin: 0px; border-radius: 3px; font-size: 10px;";
    m_systemLabel->setStyleSheet(labelStyle);
    m_appsLabel->setStyleSheet(labelStyle);
    m_mediaLabel->setStyleSheet(labelStyle);
    m_othersLabel->setStyleSheet(labelStyle);
    m_freeLabel->setStyleSheet(labelStyle);

    m_legendLayout->addWidget(m_systemLabel);
    m_legendLayout->addWidget(m_appsLabel);
    m_legendLayout->addWidget(m_mediaLabel);
    m_legendLayout->addWidget(m_othersLabel);
    m_legendLayout->addWidget(m_freeLabel);
    m_legendLayout->addStretch();

    // Hide legend initially
    m_systemLabel->hide();
    m_appsLabel->hide();
    m_mediaLabel->hide();
    m_othersLabel->hide();
    m_freeLabel->hide();

    m_mainLayout->addLayout(m_legendLayout);
}

QSize DiskUsageWidget::sizeHint() const { return QSize(400, 80); }

void DiskUsageWidget::updateUI()
{
    if (m_state == Loading) {
        m_statusLabel->setText("Loading disk usage...");
        m_statusLabel->show();
        m_diskBarContainer->hide();
        m_systemLabel->hide();
        m_appsLabel->hide();
        m_mediaLabel->hide();
        m_othersLabel->hide();
        m_freeLabel->hide();
        return;
    }

    if (m_state == Error) {
        m_statusLabel->setText("Error: " + m_errorMessage);
        m_statusLabel->show();
        m_diskBarContainer->hide();
        m_systemLabel->hide();
        m_appsLabel->hide();
        m_mediaLabel->hide();
        m_othersLabel->hide();
        m_freeLabel->hide();
        return;
    }

    if (m_totalCapacity == 0) {
        m_statusLabel->setText("No disk information available.");
        m_statusLabel->show();
        m_diskBarContainer->hide();
        m_systemLabel->hide();
        m_appsLabel->hide();
        m_mediaLabel->hide();
        m_othersLabel->hide();
        m_freeLabel->hide();
        return;
    }

    // Hide status label and show disk bar and legend
    m_statusLabel->hide();
    m_diskBarContainer->show();
    m_systemLabel->show();
    m_appsLabel->show();
    m_mediaLabel->show();
    m_othersLabel->show();
    m_freeLabel->show();

    // Calculate proportions for each segment
    int totalWidth = m_diskBarContainer->width();

    int systemWidth =
        (int)((double)m_systemUsage / m_totalCapacity * totalWidth);
    int appsWidth = (int)((double)m_appsUsage / m_totalCapacity * totalWidth);
    int mediaWidth = (int)((double)m_mediaUsage / m_totalCapacity * totalWidth);
    int othersWidth =
        (int)((double)m_othersUsage / m_totalCapacity * totalWidth);
    int freeWidth = (int)((double)m_freeSpace / m_totalCapacity * totalWidth);

    // Ensure at least 1 pixel width for non-zero values
    if (m_systemUsage > 0 && systemWidth == 0)
        systemWidth = 1;
    if (m_appsUsage > 0 && appsWidth == 0)
        appsWidth = 1;
    if (m_mediaUsage > 0 && mediaWidth == 0)
        mediaWidth = 1;
    if (m_othersUsage > 0 && othersWidth == 0)
        othersWidth = 1;
    if (m_freeSpace > 0 && freeWidth == 0)
        freeWidth = 1;

    m_diskBarLayout->setStretchFactor(m_systemBar, systemWidth);
    m_diskBarLayout->setStretchFactor(m_appsBar, appsWidth);
    m_diskBarLayout->setStretchFactor(m_mediaBar, mediaWidth);
    m_diskBarLayout->setStretchFactor(m_othersBar, othersWidth);
    m_diskBarLayout->setStretchFactor(m_freeBar, freeWidth);

    // Hide segments with zero usage
    // m_systemBar->setVisible(m_systemUsage > 0);
    // m_appsBar->setVisible(m_appsUsage > 0);
    // m_mediaBar->setVisible(m_mediaUsage > 0);
    // m_othersBar->setVisible(m_othersUsage > 0);
    // m_freeBar->setVisible(m_freeSpace > 0);

    // Format sizes for display
    auto formatSize = [](uint64_t bytes) -> QString {
        const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = bytes;

        while (size >= 1024 && unitIndex < 4) {
            size /= 1024;
            unitIndex++;
        }

        return QString("%1 %2")
            .arg(QString::number(size, 'f', 1))
            .arg(units[unitIndex]);
    };

    // Update legend labels with sizes
    m_systemLabel->setText(
        QString("System (%1)").arg(formatSize(m_systemUsage)));
    m_appsLabel->setText(QString("Apps (%1)").arg(formatSize(m_appsUsage)));
    m_mediaLabel->setText(QString("Media (%1)").arg(formatSize(m_mediaUsage)));
    m_othersLabel->setText(
        QString("Others (%1)").arg(formatSize(m_othersUsage)));
    m_freeLabel->setText(QString("Free (%1)").arg(formatSize(m_freeSpace)));

    qDebug() << "Disk Usage Updated:"
             << "System:" << m_systemUsage << "Apps:" << m_appsUsage
             << "Media:" << m_mediaUsage << "Others:" << m_othersUsage
             << "Free:" << m_freeSpace;

    // Set stretch factors and ensure minimum visibility
    int systemStretch = std::max(
        1, (int)(m_systemUsage / 1000000)); // Convert to MB for stretch
    int appsStretch = std::max(1, (int)(m_appsUsage / 1000000));
    int mediaStretch = std::max(1, (int)(m_mediaUsage / 1000000));
    int othersStretch = std::max(1, (int)(m_othersUsage / 1000000));
    int freeStretch = std::max(1, (int)(m_freeSpace / 1000000));

    m_diskBarLayout->setStretchFactor(m_systemBar, systemStretch);
    m_diskBarLayout->setStretchFactor(m_appsBar, appsStretch);
    m_diskBarLayout->setStretchFactor(m_mediaBar, mediaStretch);
    m_diskBarLayout->setStretchFactor(m_othersBar, othersStretch);
    m_diskBarLayout->setStretchFactor(m_freeBar, freeStretch);

    // Set usage info for popovers
#ifdef Q_OS_MAC
    m_systemBar->setUsageInfo("System", formatSize(m_systemUsage), "#a1384d",
                              (double)m_systemUsage / m_totalCapacity);
    m_appsBar->setUsageInfo("Apps", formatSize(m_appsUsage), "#3498DB",
                            (double)m_appsUsage / m_totalCapacity);
    m_mediaBar->setUsageInfo("Media", formatSize(m_mediaUsage), "#2ECC71",
                             (double)m_mediaUsage / m_totalCapacity);
    m_othersBar->setUsageInfo("Others", formatSize(m_othersUsage), "#F39C12",
                              (double)m_othersUsage / m_totalCapacity);
    m_freeBar->setUsageInfo("Free", formatSize(m_freeSpace), "#BDC3C7",
                            (double)m_freeSpace / m_totalCapacity);
#endif
    // Hide segments with zero usage
    // m_systemBar->setVisible(m_systemUsage > 0);
    // m_appsBar->setVisible(m_appsUsage > 0);
    // m_mediaBar->setVisible(m_mediaUsage > 0);
    // m_othersBar->setVisible(m_othersUsage > 0);
    // m_freeBar->setVisible(m_freeSpace > 0);
}

void DiskUsageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // No custom painting needed - using widgets and layouts
}

void DiskUsageWidget::fetchData()
{
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher]() {
                QVariantMap result = watcher->result();
                if (result.contains("error")) {
                    m_state = Error;
                    m_errorMessage = result["error"].toString();
                } else {
                    m_totalCapacity = result["totalCapacity"].toULongLong();
                    m_systemUsage = result["systemUsage"].toULongLong();
                    m_appsUsage = result["appsUsage"].toULongLong();
                    m_mediaUsage = result["mediaUsage"].toULongLong();
                    m_freeSpace = result["freeSpace"].toULongLong();

                    uint64_t usedKnown =
                        m_systemUsage + m_appsUsage + m_mediaUsage;
                    if (m_totalCapacity > (m_freeSpace + usedKnown)) {
                        m_othersUsage =
                            m_totalCapacity - m_freeSpace - usedKnown;
                    } else {
                        m_othersUsage = 0;
                    }

                    m_state = Ready;
                }
                updateUI(); // Update the UI instead of triggering repaint
                watcher->deleteLater();
            });

    QFuture<QVariantMap> future = QtConcurrent::run([this]() {
        QVariantMap result;
        if (!m_device || !m_device->device) {
            result["error"] = "Invalid device.";
            return result;
        }

        result["totalCapacity"] = QVariant::fromValue(
            m_device->deviceInfo.diskInfo.totalDiskCapacity);
        result["freeSpace"] = QVariant::fromValue(
            m_device->deviceInfo.diskInfo.totalDataAvailable);
        result["systemUsage"] = QVariant::fromValue(
            m_device->deviceInfo.diskInfo.totalSystemCapacity);

        // Apps usage
        uint64_t totalAppsSpace = 0;
        instproxy_client_t instproxy = nullptr;
        lockdownd_client_t lockdownClient = nullptr;
        lockdownd_service_descriptor_t lockdowndService = nullptr;

        if (lockdownd_client_new_with_handshake(m_device->device,
                                                &lockdownClient, APP_LABEL) !=
            LOCKDOWN_E_SUCCESS) {
            result["error"] = "Could not connect to lockdown service.";
            return result;
        }

        if (lockdownd_start_service(lockdownClient,
                                    "com.apple.mobile.installation_proxy",
                                    &lockdowndService) != LOCKDOWN_E_SUCCESS) {
            result["error"] = "Could not start installation proxy service.";
            lockdownd_client_free(lockdownClient);
            return result;
        }

        if (instproxy_client_new(m_device->device, lockdowndService,
                                 &instproxy) != INSTPROXY_E_SUCCESS) {
            result["error"] = "Could not connect to installation proxy.";
            lockdownd_service_descriptor_free(lockdowndService);
            lockdownd_client_free(lockdownClient);
            return result;
        }

        // The service descriptor is no longer needed after the client is
        // created
        lockdownd_service_descriptor_free(lockdowndService);
        lockdowndService = nullptr;

        plist_t client_opts = instproxy_client_options_new();
        plist_dict_set_item(client_opts, "ApplicationType",
                            plist_new_string("User"));

        plist_t return_attrs = plist_new_array();
        plist_array_append_item(return_attrs,
                                plist_new_string("StaticDiskUsage"));
        plist_array_append_item(return_attrs,
                                plist_new_string("DynamicDiskUsage"));
        plist_dict_set_item(client_opts, "ReturnAttributes", return_attrs);

        plist_t apps = nullptr;
        if (instproxy_browse(instproxy, client_opts, &apps) ==
                INSTPROXY_E_SUCCESS &&
            apps) {
            if (plist_get_node_type(apps) == PLIST_ARRAY) {
                for (uint32_t i = 0; i < plist_array_get_size(apps); i++) {
                    plist_t app_info = plist_array_get_item(apps, i);
                    if (!app_info)
                        continue;

                    plist_t static_usage =
                        plist_dict_get_item(app_info, "StaticDiskUsage");
                    if (static_usage &&
                        plist_get_node_type(static_usage) == PLIST_UINT) {
                        uint64_t static_size = 0;
                        plist_get_uint_val(static_usage, &static_size);
                        totalAppsSpace += static_size;
                    }

                    plist_t dynamic_usage =
                        plist_dict_get_item(app_info, "DynamicDiskUsage");
                    if (dynamic_usage &&
                        plist_get_node_type(dynamic_usage) == PLIST_UINT) {
                        uint64_t dynamic_size = 0;
                        plist_get_uint_val(dynamic_usage, &dynamic_size);
                        totalAppsSpace += dynamic_size;
                    }
                }
            }
            plist_free(apps);
        }
        result["appsUsage"] = QVariant::fromValue(totalAppsSpace);
        plist_free(client_opts);
        instproxy_client_free(instproxy);

        // Media usage
        uint64_t mediaSpace = 0;
        plist_t node = nullptr;
        if (lockdownd_get_value(lockdownClient, "com.apple.mobile.iTunes",
                                nullptr, &node) == LOCKDOWN_E_SUCCESS &&
            node) {
            plist_t mediaNode = plist_dict_get_item(node, "MediaLibrarySize");
            if (mediaNode && plist_get_node_type(mediaNode) == PLIST_UINT) {
                plist_get_uint_val(mediaNode, &mediaSpace);
            }
            plist_free(node);
        }
        result["mediaUsage"] = QVariant::fromValue(mediaSpace);

        lockdownd_client_free(lockdownClient);
        return result;
    });
    watcher->setFuture(future);
}
