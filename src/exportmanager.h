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

#ifndef EXPORTMANAGER_H
#define EXPORTMANAGER_H

#include "exportmanagerthread.h"
#include "iDescriptor.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QUuid>
#include <atomic>
#include <memory>
#include <optional>

// Forward declaration
class ExportProgressDialog;

class ExportManager : public QObject
{
    Q_OBJECT

public:
    // Singleton access method
    static ExportManager *sharedInstance();

    // Delete copy and assignment operators
    ExportManager(const ExportManager &) = delete;
    ExportManager &operator=(const ExportManager &) = delete;

    QUuid startExport(iDescriptorDevice *device, const QList<ExportItem> &items,
                      const QString &destinationPath,
                      std::optional<AfcClientHandle *> altAfc = std::nullopt);

    void cancelExport(const QUuid &jobId);

    bool isJobRunning(const QUuid &jobId) const;
    static QString generateUniqueOutputPath(const QString &basePath);

    // todo: should we delete this in ~ExportManager?
    ExportManagerThread *m_exportThread = new ExportManagerThread(this);
signals:

    void exportStarted(const QUuid &jobId, int totalItems,
                       const QString &destinationPath);

    void exportProgress(const QUuid &jobId, int currentItem, int totalItems,
                        const QString &currentFileName);

    void fileTransferProgress(const QUuid &jobId, const QString &fileName,
                              qint64 bytesTransferred, qint64 totalFileSize);

    void itemExported(const QUuid &jobId, const ExportResult &result);

    void exportFinished(const QUuid &jobId, const ExportJobSummary &summary);

    void exportCancelled(const QUuid &jobId);

private:
    // Private constructor for singleton pattern
    explicit ExportManager(QObject *parent = nullptr);
    ~ExportManager();

    void executeExportJob(ExportJob *job);

    QString extractFileName(const QString &devicePath) const;

    void cleanupJob(const QUuid &jobId);

    // Thread-safe storage for active jobs
    mutable QMutex m_jobsMutex;
    QMap<QUuid, ExportJob *> m_activeJobs;

    // Manager owns the dialog
    ExportProgressDialog *m_exportProgressDialog;
};

#endif // EXPORTMANAGER_H