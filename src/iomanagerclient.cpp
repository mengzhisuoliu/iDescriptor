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

#include "iomanagerclient.h"
#include "statusballoon.h"

IOManagerClient *IOManagerClient::sharedInstance()
{
    static IOManagerClient self;
    return &self;
}
IOManagerClient::IOManagerClient(QObject *parent) : QObject(parent) {}

void IOManagerClient::startExport(
    const std::shared_ptr<iDescriptorDevice> device,
    const QList<QString> &items, const QString &destinationPath,
    const QString &exportTitle, std::optional<bool> altAfc)
{
    qDebug() << "startExport() entry - items:" << items.size()
             << "dest:" << destinationPath;
    if (!device) {
        qWarning() << "Invalid device provided to ExportManager";
        QMessageBox::critical(nullptr, "Export Error",
                              "Invalid device specified for export.");
        return;
    }

    if (items.isEmpty()) {
        qWarning() << "No items provided for export";
        QMessageBox::information(nullptr, "Export Error",
                                 "No items selected for export.");
        return;
    }

    QDir destDir(destinationPath);
    if (!destDir.exists()) {
        if (!destDir.mkpath(".")) {
            qWarning() << "Could not create destination directory:"
                       << destinationPath;
            QMessageBox::critical(nullptr, "Export Error",
                                  "Could not create destination directory.");

            return;
        }
    }

    QUuid jobId = QUuid::createUuid();

    StatusBalloon::sharedInstance()->startProcess(
        exportTitle, items.size(), destinationPath, ProcessType::Export, jobId);

    AppContext::sharedInstance()->ioManager->start_export(
        device->udid, jobId, items, destinationPath);

    qDebug() << "Started export job" << jobId << "for" << items.size()
             << "items";
}

void IOManagerClient::startImport(
    const std::shared_ptr<iDescriptorDevice> device,
    const QList<QString> &items, const QString &destinationPath,
    const QString &importTitle, std::optional<bool> altAfc)
{
    qDebug() << "startExport() entry - items:" << items.size()
             << "dest:" << destinationPath;
    if (!device) {
        qWarning() << "Invalid device provided to ExportManager";
        QMessageBox::critical(nullptr, "Import Error",
                              "Invalid device specified for import.");
        return;
    }

    if (items.isEmpty()) {
        qWarning() << "No items provided for export";
        QMessageBox::information(nullptr, "Import Error",
                                 "No items selected for import.");
        return;
    }

    QUuid jobId = QUuid::createUuid();

    StatusBalloon::sharedInstance()->startProcess(
        importTitle, items.size(), destinationPath, ProcessType::Import, jobId);

    AppContext::sharedInstance()->ioManager->start_import(
        device->udid, jobId, items, destinationPath);

    qDebug() << "Started import job" << jobId << "for" << items.size()
             << "items";
}

void IOManagerClient::cancel(const QUuid &jobId)
{
    AppContext::sharedInstance()->ioManager->cancel_job(jobId);
}

void IOManagerClient::cancelAllJobs()
{
    AppContext::sharedInstance()->ioManager->cancel_all_jobs();
}