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

#include "exportmanager.h"
#include "exportprogressdialog.h"
#include "servicemanager.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>

ExportManager *ExportManager::sharedInstance()
{
    static ExportManager self;
    return &self;
}

ExportManager::ExportManager(QObject *parent) : QObject(parent)
{
    // The singleton now creates and owns the dialog.
    // No parent is passed, so it's a top-level window.
    m_exportProgressDialog = new ExportProgressDialog(this, nullptr);
}

ExportManager::~ExportManager()
{
    // Cancel all active jobs
    QMutexLocker locker(&m_jobsMutex);
    for (auto jobPtr : m_activeJobs) {
        jobPtr->cancelRequested = true;
        if (jobPtr->watcher) {
            jobPtr->watcher->cancel();
            jobPtr->watcher->waitForFinished();
        }
        delete jobPtr;
    }
    m_activeJobs.clear();

    // The dialog will be deleted automatically due to parent-child relationship
}

QUuid ExportManager::startExport(iDescriptorDevice *device,
                                 const QList<ExportItem> &items,
                                 const QString &destinationPath,
                                 std::optional<afc_client_t> altAfc)
{
    if (!device || !device->mutex) {
        qWarning() << "Invalid device provided to ExportManager";
        return QUuid();
    }

    if (items.isEmpty()) {
        qWarning() << "No items provided for export";
        return QUuid();
    }

    // Validate destination directory
    QDir destDir(destinationPath);
    if (!destDir.exists()) {
        if (!destDir.mkpath(".")) {
            qWarning() << "Could not create destination directory:"
                       << destinationPath;
            return QUuid();
        }
    }

    // Create new job
    auto job = new ExportJob();
    job->jobId = QUuid::createUuid();
    job->device = device;
    job->items = items;
    job->destinationPath = destinationPath;
    job->altAfc = altAfc;
    job->watcher = new QFutureWatcher<void>(this);

    const QUuid jobId = job->jobId;

    connect(job->watcher, &QFutureWatcher<void>::finished, this,
            [this, jobId]() { cleanupJob(jobId); });

    // Store job before starting
    {
        QMutexLocker locker(&m_jobsMutex);
        m_activeJobs[jobId] = job;
    }

    emit exportStarted(jobId, items.size(), destinationPath);

    // The manager now shows its own dialog
    m_exportProgressDialog->showForJob(jobId);

    ExportJob *jobPtr = m_activeJobs[jobId];
    jobPtr->future =
        QtConcurrent::run([this, jobPtr]() { executeExportJob(jobPtr); });
    jobPtr->watcher->setFuture(jobPtr->future);

    qDebug() << "Started export job" << jobId << "for" << items.size()
             << "items";
    return jobId;
}

void ExportManager::cancelExport(const QUuid &jobId)
{
    QMutexLocker locker(&m_jobsMutex);
    auto it = m_activeJobs.find(jobId);
    if (it != m_activeJobs.end()) {
        it.value()->cancelRequested = true;
        qDebug() << "Cancellation requested for job" << jobId;
    }
}

bool ExportManager::isExporting() const
{
    QMutexLocker locker(&m_jobsMutex);
    return !m_activeJobs.isEmpty();
}

bool ExportManager::isJobRunning(const QUuid &jobId) const
{
    QMutexLocker locker(&m_jobsMutex);
    return m_activeJobs.contains(jobId);
}

void ExportManager::executeExportJob(ExportJob *job)
{
    ExportJobSummary summary;
    summary.jobId = job->jobId;
    summary.totalItems = job->items.size();
    summary.destinationPath = job->destinationPath;

    qDebug() << "Executing export job" << job->jobId << "with"
             << job->items.size() << "items";

    for (int i = 0; i < job->items.size(); ++i) {
        // Check for cancellation
        if (job->cancelRequested.load()) {
            summary.wasCancelled = true;
            qDebug() << "Export job" << job->jobId << "was cancelled";
            emit exportCancelled(job->jobId);
            return;
        }

        const ExportItem &item = job->items.at(i);

        emit exportProgress(job->jobId, i + 1, job->items.size(),
                            item.suggestedFileName);

        ExportResult result =
            exportSingleItem(job->device, item, job->destinationPath,
                             job->altAfc, job->cancelRequested, job->jobId);

        if (result.success) {
            summary.successfulItems++;
            summary.totalBytesTransferred += result.bytesTransferred;
        } else {
            summary.failedItems++;
        }

        emit itemExported(job->jobId, result);

        // Check for cancellation again after potentially long file operation
        if (job->cancelRequested.load()) {
            summary.wasCancelled = true;
            qDebug() << "Export job" << job->jobId
                     << "was cancelled during execution";
            emit exportCancelled(job->jobId);
            return;
        }
    }

    qDebug() << "Export job" << job->jobId
             << "completed - Success:" << summary.successfulItems
             << "Failed:" << summary.failedItems
             << "Bytes:" << summary.totalBytesTransferred;

    emit exportFinished(job->jobId, summary);
}

ExportResult ExportManager::exportSingleItem(iDescriptorDevice *device,
                                             const ExportItem &item,
                                             const QString &destinationDir,
                                             std::optional<afc_client_t> altAfc,
                                             std::atomic<bool> &cancelRequested,
                                             const QUuid &jobId)
{
    ExportResult result;
    result.sourceFilePath = item.sourcePathOnDevice;

    // Generate output path
    QString outputPath = QDir(destinationDir).filePath(item.suggestedFileName);
    outputPath = generateUniqueOutputPath(outputPath);
    result.outputFilePath = outputPath;
    QDateTime modificationTime;
    QDateTime birthTime;
    // Get file size first
    //  Example {
    //   "st_size": 64523,
    //   "st_blocks": 128,
    //   "st_nlink": 1,
    //   "st_ifmt": "S_IFREG",
    //   "st_mtime": 1754987735634348907,
    //   "st_birthtime": 1754987735633715011
    // }

    plist_t info = nullptr;
    afc_error_t infoResult = ServiceManager::safeAfcGetFileInfoPlist(
        device, item.sourcePathOnDevice.toUtf8().constData(), &info, altAfc);
    quint64 totalFileSize = 0;
    if (infoResult != AFC_E_SUCCESS || !info) {
        qDebug() << "File info retrieval failed for" << item.sourcePathOnDevice;
        return result;
    }

    PlistNavigator fileInfo = PlistNavigator(info);

    bool valid = fileInfo["st_size"].valid();
    if (!valid) {
        qDebug() << "File size info not valid for" << item.sourcePathOnDevice;
        if (info)
            plist_free(info);
        return result;
    }

    totalFileSize = fileInfo["st_size"].getUInt();

    valid = fileInfo["st_mtime"].valid();
    if (!valid) {
        qDebug() << "File modification time info not valid for"
                 << item.sourcePathOnDevice;
        if (info)
            plist_free(info);
        return result;
    }

    uint64_t modTimeNs = fileInfo["st_mtime"].getUInt();
    // The timestamp from the device is in nanoseconds, convert to seconds
    modificationTime = QDateTime::fromSecsSinceEpoch(modTimeNs / 1000000000);

    valid = fileInfo["st_birthtime"].valid();
    if (!valid) {
        qDebug() << "File birth time info not valid for"
                 << item.sourcePathOnDevice;
        if (info)
            plist_free(info);
        return result;
    }
    uint64_t birthTimeNs = fileInfo["st_birthtime"].getUInt();
    birthTime = QDateTime::fromSecsSinceEpoch(birthTimeNs / 1000000000);

    plist_free(info);

    // Open file on device
    uint64_t handle = 0;
    afc_error_t openResult = ServiceManager::safeAfcFileOpen(
        device, item.sourcePathOnDevice.toUtf8().constData(), AFC_FOPEN_RDONLY,
        &handle, altAfc);

    if (openResult != AFC_E_SUCCESS) {
        result.errorMessage =
            QString("Failed to open file on device: %1 (AFC error: %2)")
                .arg(item.sourcePathOnDevice)
                .arg(static_cast<int>(openResult));
        return result;
    }

    // Open local output file
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        result.errorMessage = QString("Failed to create local file: %1 (%2)")
                                  .arg(outputPath)
                                  .arg(outputFile.errorString());
        ServiceManager::safeAfcFileClose(device, handle, altAfc);
        return result;
    }

    char buffer[8192];
    uint32_t bytesRead = 0;
    quint64 totalBytes = 0;

    while (true) {
        // Check for cancellation during file copy
        if (cancelRequested.load()) {
            outputFile.close();
            outputFile.remove(); // Clean up partial file
            ServiceManager::safeAfcFileClose(device, handle, altAfc);
            result.errorMessage = "Export cancelled by user";
            return result;
        }

        afc_error_t readResult = ServiceManager::safeAfcFileRead(
            device, handle, buffer, sizeof(buffer), &bytesRead, altAfc);

        if (readResult != AFC_E_SUCCESS || bytesRead == 0) {
            break; // End of file or error
        }

        qint64 bytesWritten = outputFile.write(buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            result.errorMessage =
                QString("Write error: only wrote %1 of %2 bytes")
                    .arg(bytesWritten)
                    .arg(bytesRead);
            outputFile.close();
            outputFile.remove(); // Clean up partial file
            ServiceManager::safeAfcFileClose(device, handle, altAfc);
            return result;
        }

        totalBytes += bytesRead;

        // Emit progress update every 64KB or at end of file
        if (totalBytes % (64 * 1024) == 0 || totalBytes == totalFileSize) {
            emit fileTransferProgress(jobId, item.suggestedFileName, totalBytes,
                                      totalFileSize);
        }
    }

    outputFile.close();
    ServiceManager::safeAfcFileClose(device, handle, altAfc);

    // reopen is required for timestamps
    QFile reopen(outputPath);
    reopen.open(QIODevice::ReadOnly);
    if (modificationTime.isValid()) {
        if (!reopen.setFileTime(modificationTime,
                                QFileDevice::FileModificationTime)) {
            qWarning() << "Could not set modification time for" << outputPath;
        }
    }
    if (birthTime.isValid()) {
        // fails on linux
        if (!reopen.setFileTime(birthTime, QFileDevice::FileBirthTime)) {
            qWarning() << "Could not set birth time for" << outputPath;
        }
    }

    if (totalBytes == 0) {
        result.errorMessage = "No data read from device file";
        outputFile.remove(); // Clean up empty file
        return result;
    }

    result.success = true;
    result.bytesTransferred = totalBytes;
    return result;
}

QString ExportManager::generateUniqueOutputPath(const QString &basePath) const
{
    if (!QFile::exists(basePath)) {
        return basePath;
    }

    QFileInfo fileInfo(basePath);
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    QString directory = fileInfo.absolutePath();

    int counter = 1;
    QString uniquePath;

    do {
        QString newName = QString("%1_%2").arg(baseName).arg(counter);
        if (!suffix.isEmpty()) {
            newName += "." + suffix;
        }
        uniquePath = QDir(directory).filePath(newName);
        counter++;
    } while (QFile::exists(uniquePath) && counter < 10000);

    return uniquePath;
}

QString ExportManager::extractFileName(const QString &devicePath) const
{
    int lastSlash = devicePath.lastIndexOf('/');
    if (lastSlash != -1 && lastSlash < devicePath.length() - 1) {
        return devicePath.mid(lastSlash + 1);
    }
    return devicePath;
}

void ExportManager::cleanupJob(const QUuid &jobId)
{
    QMutexLocker locker(&m_jobsMutex);
    auto it = m_activeJobs.find(jobId);
    if (it != m_activeJobs.end()) {
        if (it.value()->watcher) {
            it.value()->watcher->deleteLater();
        }

        delete it.value();
        m_activeJobs.erase(it);
        qDebug() << "Cleaned up export job" << jobId;
    }
}
