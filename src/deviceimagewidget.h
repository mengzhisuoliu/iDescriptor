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

#ifndef DEVICEIMAGEWIDGET_H
#define DEVICEIMAGEWIDGET_H

#include "iDescriptor.h"
#include "responsiveqlabel.h"
#include <QTimer>
#include <QWidget>

class DeviceImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceImageWidget(const std::shared_ptr<iDescriptorDevice> device,
                               QWidget *parent = nullptr);
    ~DeviceImageWidget();

private slots:
    void updateTime();

private:
    QString m_mockupName;
    void setupDeviceImage();
    QString getDeviceMockupPath() const;
    QString getWallpaperPath() const;
    QString getMockupNameFromDisplayName(const QString &displayName) const;
    int getIosVersionFromDevice() const;
    QPixmap createCompositeImage() const;
    QRect findScreenArea(const QPixmap &mockup) const;

    const std::shared_ptr<iDescriptorDevice> m_device;
    ResponsiveQLabel *m_imageLabel;
    QTimer *m_timeUpdateTimer;

    QString m_mockupPath;
    QString m_wallpaperPath;
};

#endif // DEVICEIMAGEWIDGET_H