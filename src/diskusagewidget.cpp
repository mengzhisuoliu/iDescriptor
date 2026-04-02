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

#include "diskusagewidget.h"
#include "diskusagebar.h"
#include "iDescriptor.h"
extern "C" {
#include <sqlite3.h>
}

#include <QApplication>
#include <QDebug>
#include <QFutureWatcher>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrent>

using namespace iDescriptor;

DiskUsageWidget::DiskUsageWidget(
    const std::shared_ptr<iDescriptorDevice> device, QWidget *parent)
    : QWidget(parent), m_device(device), m_state(Loading), m_totalCapacity(0),
      m_systemUsage(0), m_appsUsage(0), m_mediaUsage(0), m_othersUsage(0),
      m_freeSpace(0)
{
    setMinimumHeight(80);
    setupUI();
    connect(m_device->service_manager,
            &CXX::ServiceManager::disk_usage_retrieved, this,
            &DiskUsageWidget::onDiskUsageRetrieved);
    QTimer::singleShot(100, this, [this] {
        /*
            on older devices if Photos.sqlite is high in size and the device is
            connected wirelessly it takes ~5 minutes to read the entire file
            maybe skip on wireless connections on old devices than iPhone10,1
            (iPhone 8)
        */
        bool skipGalleryUsage =
            m_device->deviceInfo.is_iPhone && m_device->deviceInfo.isWireless &&
            !iDescriptor::Utils::isProductTypeNewer(
                m_device->deviceInfo.rawProductType, "iPhone10,1");
        if (skipGalleryUsage)
            qDebug() << "Skipping gallery usage calculation";
        m_device->service_manager->fetch_disk_usage(skipGalleryUsage);
    });
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

    // Stacked widget for different states
    m_stackedWidget = new QStackedWidget(this);
    m_mainLayout->addWidget(m_stackedWidget);

    // Loading/Error page
    m_loadingErrorPage = new QWidget();
    m_loadingErrorLayout = new QVBoxLayout(m_loadingErrorPage);
    m_loadingErrorLayout->setContentsMargins(0, 0, 0, 0);
    m_loadingErrorLayout->setSpacing(5);

    m_processIndicator = new QProcessIndicator(m_loadingErrorPage);
    m_processIndicator->setFixedSize(24, 24);
    m_processIndicator->start();

    m_statusLabel = new QLabel(m_loadingErrorPage);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setText("Loading disk usage...");

    m_loadingErrorLayout->addStretch();
    m_loadingErrorLayout->addWidget(m_processIndicator, 0, Qt::AlignCenter);
    m_loadingErrorLayout->addWidget(m_statusLabel);
    m_loadingErrorLayout->addStretch();

    m_stackedWidget->addWidget(m_loadingErrorPage);

    // Data page
    m_dataPage = new QWidget();
    m_dataLayout = new QVBoxLayout(m_dataPage);
    m_dataLayout->setContentsMargins(0, 0, 0, 0);
    m_dataLayout->setSpacing(0);

    // Disk usage bar container
    m_diskBarContainer = new QWidget(this);
    m_diskBarContainer->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Fixed);
    m_diskBarContainer->setFixedHeight(20);
    m_diskBarLayout = new QHBoxLayout(m_diskBarContainer);
    m_diskBarLayout->setContentsMargins(0, 0, 0, 0);
    m_diskBarLayout->setSpacing(0);

    /*
        FIXME: There is a bug with qt, related to NSPopover on macOS
        need to revisit this when we find a fix
    */
    // #ifdef Q_OS_MAC
    //     m_systemBar = new DiskUsageBar();
    //     m_appsBar = new DiskUsageBar();
    //     m_mediaBar = new DiskUsageBar();
    //     m_othersBar = new DiskUsageBar();
    //     m_freeBar = new DiskUsageBar();

    //     m_systemBar->setStyleSheet(
    //         " background-color: #a1384d; border: 1px solid"
    //         "#e64a5b; padding: 0; margin: 0; border-top-left-radius: 3px; "
    //         "border-bottom-left-radius: 3px; ");
    //     m_appsBar->setStyleSheet("background-color: #4f869f; border: 1px
    //     solid "
    //                              "#63b4da; padding: 0; margin: 0; ");
    //     m_mediaBar->setStyleSheet("background-color: #2ECC71; "
    //                               "border: none; padding: 0; margin: 0; ");
    //     m_othersBar->setStyleSheet("background-color: #a28729; border: 1px
    //     solid "
    //                                "#c4a32d; padding: 0; margin: 0; ");
    //     m_freeBar->setStyleSheet(
    //         "background-color: #6e6d6d; border: 1px solid "
    //         "#4f4f4f; padding: 0; margin: 0; border-top-right-radius: 3px; "
    //         "border-bottom-right-radius: 3px; ");

    m_systemBar = new QWidget();
    m_appsBar = new QWidget();
    m_mediaBar = new QWidget();
    m_galleryBar = new QWidget();
    m_othersBar = new QWidget();
    m_freeBar = new QWidget();

    // required for tooltips to have default styling
    m_systemBar->setObjectName("systemBar");
    m_appsBar->setObjectName("appsBar");
    m_mediaBar->setObjectName("mediaBar");
    m_galleryBar->setObjectName("galleryBar");
    m_othersBar->setObjectName("othersBar");
    m_freeBar->setObjectName("freeBar");

    bool dark = isDarkMode();

    // Set colors
    m_systemBar->setStyleSheet(
        "QWidget#systemBar { background-color: #a1384d; border: 1px solid"
        "#e64a5b; padding: 0; margin: 0; border-radius:0px; "
        "border-top-left-radius: 3px; "
        "border-bottom-left-radius: 3px; }");
    m_appsBar->setStyleSheet(
        "QWidget#appsBar { background-color: #4f869f; border: 1px solid "
        "#63b4da;  border-radius:0px; padding: 0; margin: 0; }");
    m_mediaBar->setStyleSheet("QWidget#mediaBar { background-color: #2ECC71; "
                              "border: none; padding: 0; margin: 0; }");
    m_galleryBar->setStyleSheet(
        "QWidget#galleryBar { background-color: #9b59b6; border: 1px solid "
        "#8e44ad;  border-radius:0px; padding: 0; margin: 0; }");
    m_othersBar->setStyleSheet(
        "QWidget#othersBar { background-color: #a28729; border: 1px solid "
        "#c4a32d;  border-radius:0px; padding: 0; margin: 0; }");
    m_freeBar->setStyleSheet(
        QString("QWidget#freeBar { background-color: %1; border: "
                "1px solid "
                "#4f4f4f4f; padding: 0; margin: 0; border-radius:0px; "
                "border-top-right-radius: 3px; "
                "border-bottom-right-radius: 3px; }")
            .arg(dark ? "rgba(255, 255, 255, 10)" : "rgba(0, 0, 0, 25)"));

    // remove padding margin from layout
    m_systemBar->setContentsMargins(0, 0, 0, 0);
    m_appsBar->setContentsMargins(0, 0, 0, 0);
    m_mediaBar->setContentsMargins(0, 0, 0, 0);
    m_galleryBar->setContentsMargins(0, 0, 0, 0);
    m_othersBar->setContentsMargins(0, 0, 0, 0);
    m_freeBar->setContentsMargins(0, 0, 0, 0);

    m_diskBarLayout->addWidget(m_systemBar);
    m_diskBarLayout->addWidget(m_appsBar);
    m_diskBarLayout->addWidget(m_mediaBar);
    m_diskBarLayout->addWidget(m_galleryBar);
    m_diskBarLayout->addWidget(m_othersBar);
    m_diskBarLayout->addWidget(m_freeBar);

    m_dataLayout->addWidget(m_diskBarContainer);

    QWidget *m_legendWidget = new QWidget();
    m_legendWidget->setContentsMargins(0, 0, 0, 0);
    m_legendWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_legendLayout = new QHBoxLayout(m_legendWidget);
    m_legendLayout->setSpacing(0);
    m_legendLayout->setContentsMargins(0, 0, 0, 0);

    m_systemLabel = new QLabel("System", m_legendWidget);
    m_appsLabel = new QLabel("Apps", m_legendWidget);
    m_mediaLabel = new QLabel("Media", m_legendWidget);
    m_galleryLabel = new QLabel("Gallery", m_legendWidget);
    m_othersLabel = new QLabel("Others", m_legendWidget);
    m_freeLabel = new QLabel("Free", m_legendWidget);

    QString labelStyle =
        "margin: 0px; padding: 0px 4px 0px 0px; font-size: 10px;";
    m_systemLabel->setStyleSheet(labelStyle);
    m_appsLabel->setStyleSheet(labelStyle);
    m_mediaLabel->setStyleSheet(labelStyle);
    m_galleryLabel->setStyleSheet(labelStyle);
    m_othersLabel->setStyleSheet(labelStyle);
    m_freeLabel->setStyleSheet(labelStyle);

    // FIXME:switch to zloadingwidget and remove unnecessary stretches when
    // m_galleryLabel is invisible
    m_legendLayout->addWidget(m_systemLabel);
    m_legendLayout->addStretch();
    m_legendLayout->addWidget(m_appsLabel);
    m_legendLayout->addStretch();
    m_legendLayout->addWidget(m_mediaLabel);
    m_legendLayout->addStretch();
    m_legendLayout->addWidget(m_galleryLabel);
    m_legendLayout->addStretch();
    m_legendLayout->addWidget(m_othersLabel);
    m_legendLayout->addStretch();
    m_legendLayout->addWidget(m_freeLabel);

    // Add the legend widget (not the layout) to the data layout
    m_dataLayout->addWidget(m_legendWidget);

    m_stackedWidget->addWidget(m_dataPage);

    // Initially show loading page
    m_stackedWidget->setCurrentWidget(m_loadingErrorPage);
}

void DiskUsageWidget::updateUI()
{
    if (m_state == Loading) {
        m_processIndicator->start();
        m_statusLabel->setText("Loading disk usage...");
        m_stackedWidget->setCurrentWidget(m_loadingErrorPage);
        return;
    }

    if (m_state == Error) {
        m_processIndicator->stop();
        m_statusLabel->setText("Error: " + m_errorMessage);
        m_stackedWidget->setCurrentWidget(m_loadingErrorPage);
        return;
    }

    if (m_totalCapacity == 0) {
        m_processIndicator->stop();
        m_processIndicator->hide();
        m_statusLabel->setText("No disk information available.");
        m_stackedWidget->setCurrentWidget(m_loadingErrorPage);
        return;
    }

    // Show data page
    m_stackedWidget->setCurrentWidget(m_dataPage);

    // Calculate proportions for each segment
    int totalWidth = m_diskBarContainer->width();

    int systemWidth =
        (int)((double)m_systemUsage / m_totalCapacity * totalWidth);
    int appsWidth = (int)((double)m_appsUsage / m_totalCapacity * totalWidth);
    int mediaWidth = (int)((double)m_mediaUsage / m_totalCapacity * totalWidth);
    int galleryWidth =
        (int)((double)m_galleryUsage / m_totalCapacity * totalWidth);
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
    if (m_galleryUsage > 0 && galleryWidth == 0)
        galleryWidth = 1;

    m_diskBarLayout->setStretchFactor(m_systemBar, systemWidth);
    m_diskBarLayout->setStretchFactor(m_appsBar, appsWidth);
    m_diskBarLayout->setStretchFactor(m_mediaBar, mediaWidth);
    m_diskBarLayout->setStretchFactor(m_galleryBar, galleryWidth);
    m_diskBarLayout->setStretchFactor(m_othersBar, othersWidth);
    m_diskBarLayout->setStretchFactor(m_freeBar, freeWidth);

    // Hide segments with zero usage
    m_systemBar->setVisible(m_systemUsage > 0);
    m_systemLabel->setVisible(m_systemUsage > 0);

    m_appsBar->setVisible(m_appsUsage > 0);
    m_appsLabel->setVisible(m_appsUsage > 0);

    m_mediaBar->setVisible(m_mediaUsage > 0);
    m_mediaLabel->setVisible(m_mediaUsage > 0);

    m_galleryBar->setVisible(m_galleryUsage > 0);
    m_galleryLabel->setVisible(m_galleryUsage > 0);

    m_othersBar->setVisible(m_othersUsage > 0);
    m_othersLabel->setVisible(m_othersUsage > 0);

    m_freeBar->setVisible(m_freeSpace > 0);
    m_freeLabel->setVisible(m_freeSpace > 0);

    // Update legend labels with sizes
    m_systemLabel->setText(
        QString("System (%1)").arg(Utils::formatSize(m_systemUsage)));
    m_appsLabel->setText(
        QString("Apps (%1)").arg(Utils::formatSize(m_appsUsage)));
    m_mediaLabel->setText(
        QString("Media (%1)").arg(Utils::formatSize(m_mediaUsage)));
    m_othersLabel->setText(
        QString("Others (%1)").arg(Utils::formatSize(m_othersUsage)));
    m_freeLabel->setText(
        QString("Free (%1)").arg(Utils::formatSize(m_freeSpace)));
    m_galleryLabel->setText(
        QString("Gallery (%1)").arg(Utils::formatSize(m_galleryUsage)));

    qDebug() << "Disk Usage Updated:"
             << "System:" << m_systemUsage << "Apps:" << m_appsUsage
             << "Media:" << m_mediaUsage << "Others:" << m_othersUsage
             << "Gallery:" << m_galleryUsage << "Free:" << m_freeSpace;

    // Set stretch factors and ensure minimum visibility
    int systemStretch = std::max(
        1, (int)(m_systemUsage / 1000000)); // Convert to MB for stretch
    int appsStretch = std::max(1, (int)(m_appsUsage / 1000000));
    int mediaStretch = std::max(1, (int)(m_mediaUsage / 1000000));
    int galleryStretch = std::max(1, (int)(m_galleryUsage / 1000000));
    int othersStretch = std::max(1, (int)(m_othersUsage / 1000000));
    int freeStretch = std::max(1, (int)(m_freeSpace / 1000000));

    m_diskBarLayout->setStretchFactor(m_systemBar, systemStretch);
    m_diskBarLayout->setStretchFactor(m_appsBar, appsStretch);
    m_diskBarLayout->setStretchFactor(m_mediaBar, mediaStretch);
    m_diskBarLayout->setStretchFactor(m_galleryBar, galleryStretch);
    m_diskBarLayout->setStretchFactor(m_othersBar, othersStretch);
    m_diskBarLayout->setStretchFactor(m_freeBar, freeStretch);

    /* FIXME: NSPopover bug */
    // #ifdef Q_OS_MAC
    //     m_systemBar->setUsageInfo("System", formatSize(m_systemUsage),
    //     "#a1384d",
    //                               (double)m_systemUsage / m_totalCapacity);
    //     m_appsBar->setUsageInfo("Apps", formatSize(m_appsUsage), "#3498DB",
    //                             (double)m_appsUsage / m_totalCapacity);
    //     m_mediaBar->setUsageInfo("Media", formatSize(m_mediaUsage),
    //     "#2ECC71",
    //                              (double)m_mediaUsage / m_totalCapacity);
    //     m_othersBar->setUsageInfo("Others", formatSize(m_othersUsage),
    //     "#F39C12",
    //                               (double)m_othersUsage / m_totalCapacity);
    //     m_freeBar->setUsageInfo("Free", formatSize(m_freeSpace), "#BDC3C7",
    //                             (double)m_freeSpace / m_totalCapacity);
    // #else
    m_systemBar->setToolTip(
        QString("System: %1 (%2%)")
            .arg(Utils::formatSize(m_systemUsage))
            .arg(QString::number((double)m_systemUsage / m_totalCapacity * 100,
                                 'f', 1)));
    m_appsBar->setToolTip(
        QString("Apps: %1 (%2%)")
            .arg(Utils::formatSize(m_appsUsage))
            .arg(QString::number((double)m_appsUsage / m_totalCapacity * 100,
                                 'f', 1)));
    m_mediaBar->setToolTip(
        QString("Media: %1 (%2%)")
            .arg(Utils::formatSize(m_mediaUsage))
            .arg(QString::number((double)m_mediaUsage / m_totalCapacity * 100,
                                 'f', 1)));
    m_galleryBar->setToolTip(
        QString("Gallery: %1 (%2%)")
            .arg(Utils::formatSize(m_galleryUsage))
            .arg(QString::number((double)m_galleryUsage / m_totalCapacity * 100,
                                 'f', 1)));
    m_othersBar->setToolTip(
        QString("Others: %1 (%2%)")
            .arg(Utils::formatSize(m_othersUsage))
            .arg(QString::number((double)m_othersUsage / m_totalCapacity * 100,
                                 'f', 1)));
    m_freeBar->setToolTip(
        QString("Free: %1 (%2%)")
            .arg(Utils::formatSize(m_freeSpace))
            .arg(QString::number((double)m_freeSpace / m_totalCapacity * 100,
                                 'f', 1)));
}
void DiskUsageWidget::onDiskUsageRetrieved(bool success, uint64_t apps_usage,
                                           uint64_t gallery_usage)
{
    if (!success) {
        m_state = Error;
        m_errorMessage = "Failed to retrieve disk usage data.";
    } else {
        m_state = Ready;
        m_totalCapacity = m_device->deviceInfo.diskInfo.totalDiskCapacity;
        qDebug() << "Total Capacity:" << m_totalCapacity;
        m_systemUsage = m_device->deviceInfo.diskInfo.totalSystemCapacity;
        m_appsUsage = apps_usage;
        // m_mediaUsage = result["mediaUsage"].toULongLong();
        m_mediaUsage = 0;
        m_freeSpace = m_device->deviceInfo.diskInfo.totalDataAvailable;
        // m_galleryUsage = result["galleryUsage"].toULongLong();
        m_galleryUsage = gallery_usage;

        uint64_t usedKnown =
            m_systemUsage + m_appsUsage + m_mediaUsage + m_galleryUsage;
        if (m_totalCapacity > (m_freeSpace + usedKnown)) {
            m_othersUsage = m_totalCapacity - m_freeSpace - usedKnown;
        } else {
            m_othersUsage = 0;
        }
    }

    updateUI();
}
