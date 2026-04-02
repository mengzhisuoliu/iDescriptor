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

#ifndef IOMANAGERCLIENT_H
#define IOMANAGERCLIENT_H

#include "appcontext.h"
#include "iDescriptor.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMap>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <optional>

class IOManagerClient : public QObject
{
    Q_OBJECT

public:
    static IOManagerClient *sharedInstance();

    // Delete copy and assignment operators
    IOManagerClient(const IOManagerClient &) = delete;
    IOManagerClient &operator=(const IOManagerClient &) = delete;

    void startExport(const std::shared_ptr<iDescriptorDevice> device,
                     const QList<QString> &items,
                     const QString &destinationPath, const QString &jobTitle,
                     std::optional<bool> altAfc = std::nullopt);

    void startImport(const std::shared_ptr<iDescriptorDevice> device,
                     const QList<QString> &items,
                     const QString &destinationPath, const QString &jobTitle,
                     std::optional<bool> altAfc = std::nullopt);

    void cancel(const QUuid &jobId);
    void cancelAllJobs();
    static QString generateUniqueOutputPath(const QString &basePath);

private:
    explicit IOManagerClient(QObject *parent = nullptr);
    void executeExportJob(ExportJob *job);
};

#endif // IOMANAGERCLIENT_H