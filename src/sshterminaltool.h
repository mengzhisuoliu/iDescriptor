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

#ifndef SSHTERMINALTOOL_H
#define SSHTERMINALTOOL_H

#include "networkdeviceprovider.h"
#include <QWidget>

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "sshterminalwidget.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

class SSHTerminalTool : public Tool
{
    Q_OBJECT
public:
    explicit SSHTerminalTool(QWidget *parent = nullptr);
    ~SSHTerminalTool();
private slots:
    void onOpenSSHTerminal();
    void On_iDeviceAdded(const std::shared_ptr<iDescriptorDevice> device);
    void On_iDeviceRemoved(const QString &udid);
    void onWirelessDeviceAdded(const NetworkDevice &device);
    void onWirelessDeviceRemoved(const QString &deviceName);
    void onDeviceSelected(QAbstractButton *button);
    void onManualConnectClicked();

private:
    void setupDeviceSelectionUI(QVBoxLayout *layout);
    void updateDeviceList();
    void clearDeviceButtons();
    void addDevice(const std::shared_ptr<iDescriptorDevice> device);
    void addWirelessDevice(const NetworkDevice &device);
    void resetSelection();

    QLabel *m_infoLabel;
    QPushButton *m_connectButton;

    // Device selection UI
    QVBoxLayout *m_deviceLayout;

    QGroupBox *m_connectedDevicesGroup;
    QVBoxLayout *m_connectedDevicesLayout;

    QGroupBox *m_networkDevicesGroup;
    QVBoxLayout *m_networkDevicesLayout;

    QButtonGroup *m_deviceButtonGroup;

    iDescriptor::Uniq m_selectedUniq;

    QGroupBox *m_manualConnectionGroup;
    QLineEdit *m_manualIpEdit;
    QPushButton *m_manualConnectButton;

    // SSH components
    ssh_session m_sshSession;
    ssh_channel m_sshChannel;
    QTimer *m_sshTimer;
    QProcess *iproxyProcess = nullptr;

    bool m_sshConnected = false;
    bool m_isInitialized = false;
signals:
};

#endif // SSHTERMINALTOOL_H
