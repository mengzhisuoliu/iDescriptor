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

#ifndef DEVICEMENUWIDGET_H
#define DEVICEMENUWIDGET_H
#include "deviceinfowidget.h"
#include "fileexplorerwidget.h"
#include "gallerywidget.h"
#include "iDescriptor.h"
#include "installedappswidget.h"
#include <QStackedWidget>
#include <QWidget>

class DeviceMenuWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceMenuWidget(const std::shared_ptr<iDescriptorDevice> device,
                              QWidget *parent = nullptr);
    void switchToTab(const QString &tabName);
    void init();
    ~DeviceMenuWidget();

private:
    QStackedWidget *stackedWidget;
    const std::shared_ptr<iDescriptorDevice> m_device;
    DeviceInfoWidget *m_deviceInfoWidget;
    InstalledAppsWidget *m_installedAppsWidget;
    GalleryWidget *m_galleryWidget;
    FileExplorerWidget *m_fileExplorerWidget;
signals:
};

#endif // DEVICEMENUWIDGET_H
