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

#ifndef DEVICEINFOWIDGET_H
#define DEVICEINFOWIDGET_H
#include "batterywidget.h"
#include "deviceimagewidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "servicemanager.h"
#include <QLabel>
#include <QTimer>
#include <QWidget>

class DeviceInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceInfoWidget(const iDescriptorDevice *device,
                              QWidget *parent = nullptr);
    ~DeviceInfoWidget(); // added destructor

private slots:
    void onBatteryMoreClicked();

private:
    const iDescriptorDevice *m_device;
    QTimer *m_updateTimer;
    void updateBatteryInfo();
    void updateChargingStatusIcon();
    QLabel *m_chargingStatusLabel;
    QLabel *m_chargingWattsWithCableTypeLabel;
    BatteryWidget *m_batteryWidget;
    ZIconLabel *m_lightningIconLabel;

    DeviceImageWidget *m_deviceImageWidget;
};

#endif // DEVICEINFOWIDGET_H
