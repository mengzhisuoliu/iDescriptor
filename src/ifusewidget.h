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

#ifndef IFUSEWIDGET_H
#define IFUSEWIDGET_H

#include "appcontext.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "service.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

class iFuseWidget : public Tool
{
    Q_OBJECT

public:
    static bool
    canOpenForDevice(const std::shared_ptr<iDescriptorDevice> &device);
    explicit iFuseWidget(const std::shared_ptr<iDescriptorDevice> device,
                         QWidget *parent = nullptr);

private slots:
    void onFolderPickerClicked();
    void onMountPathClicked();
    void onMountClicked();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void updateUI();

private:
    void setupUI();
    void updatePath();
    void updateDeviceComboBox();
    bool validateInputs();
    QString getSelectedDeviceUdid();
    void setStatusMessage(const QString &message, bool isError = false);
    void onDeviceChanged(const QString &deviceName);
    // UI Components
    QVBoxLayout *m_mainLayout;
    QLabel *m_descriptionLabel;
    QLabel *m_statusLabel;
    QComboBox *m_deviceComboBox;
    ZLabel *m_mountPathLabel;
    QPushButton *m_folderPickerButton;
    QLabel *m_folderNameLabel;
    QPushButton *m_mountButton;
    std::shared_ptr<iDescriptorDevice> m_device;

    // Data
    QString m_selectedPath;
    QProcess *m_ifuseProcess;
    QString m_currentMountPath;
};

#endif // IFUSEWIDGET_H