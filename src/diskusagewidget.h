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

#ifndef DISKUSAGEWIDGET_H
#define DISKUSAGEWIDGET_H
#include "diskusagebar.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "qprocessindicator.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdint>

class DiskUsageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DiskUsageWidget(const std::shared_ptr<iDescriptorDevice> device,
                             QWidget *parent = nullptr);

private:
    void onDiskUsageRetrieved(bool success, uint64_t apps_usage,
                              uint64_t gallery_usage);
    void setupUI();
    void updateUI();

    enum State { Loading, Ready, Error };

    const std::shared_ptr<iDescriptorDevice> m_device;
    State m_state;
    QString m_errorMessage;

    // UI widgets
    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QStackedWidget *m_stackedWidget;

    // Loading/Error page
    QWidget *m_loadingErrorPage;
    QVBoxLayout *m_loadingErrorLayout;
    QProcessIndicator *m_processIndicator;
    QLabel *m_statusLabel;

    // Data page
    QWidget *m_dataPage;
    QVBoxLayout *m_dataLayout;
    QWidget *m_diskBarContainer;
    QHBoxLayout *m_diskBarLayout;
    /*FIXME: NSPopover bug */
    // #ifdef Q_OS_MAC
    //     DiskUsageBar *m_systemBar;
    //     DiskUsageBar *m_appsBar;
    //     DiskUsageBar *m_mediaBar;
    //     DiskUsageBar *m_othersBar;
    //     DiskUsageBar *m_freeBar;
    // #else
    QWidget *m_systemBar;
    QWidget *m_appsBar;
    QWidget *m_mediaBar;
    QWidget *m_othersBar;
    QWidget *m_freeBar;
    QWidget *m_galleryBar;
    // #endif

    QHBoxLayout *m_legendLayout;
    QLabel *m_systemLabel;
    QLabel *m_appsLabel;
    QLabel *m_mediaLabel;
    QLabel *m_galleryLabel;
    QLabel *m_othersLabel;
    QLabel *m_freeLabel;

    uint64_t m_totalCapacity;
    uint64_t m_systemUsage;
    uint64_t m_appsUsage;
    uint64_t m_mediaUsage;
    uint64_t m_othersUsage;
    uint64_t m_freeSpace;
    uint64_t m_galleryUsage;
};

#endif // DISKUSAGEWIDGET_H
