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

#ifndef LIVESCREEN_H
#define LIVESCREEN_H

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QWidget>

class LiveScreenWidget : public Tool
{
    Q_OBJECT
public:
    explicit LiveScreenWidget(const std::shared_ptr<iDescriptorDevice> device,
                              QWidget *parent = nullptr);
    ~LiveScreenWidget();

private:
    void updateScreenshot();
    void startCapturing();
    void applyTransformAndDisplay();
    void handleFailedInitialization();

    std::shared_ptr<iDescriptorDevice> m_device;
    QLabel *m_imageLabel;
    QLabel *m_statusLabel;
    CXX::ScreenshotBackend *m_client;

    // controls for rotation / mirroring
    QWidget *m_controlsWidget = nullptr;
    QPushButton *m_rotateCwButton = nullptr;
    QPushButton *m_rotateCcwButton = nullptr;
    QPushButton *m_mirrorButton = nullptr;

    // transformation state
    QPixmap m_lastPixmap;
    int m_rotationDegrees = 0; // 0, 90, 180, 270
    bool m_mirrorHorizontal = false;

private:
    void startInitialization();
};

#endif // LIVESCREEN_H
