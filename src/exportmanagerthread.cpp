
#include "exportmanagerthread.h"
#include "appcontext.h"
#include "iDescriptor.h"
#include "servicemanager.h"
#include <QDebug>
#include <QDir>
#include <QThread>
#include <QtConcurrent>

// TODO: unfinished
void ExportManagerThread::executeExportJob(ExportJob *job)
{
    // FIXME: limit to 1 at a time per udid/device
    QtConcurrent::run([this, job]() { executeExportJobInternal(job); });
}

void ExportManagerThread::executeExportJobInternal(ExportJob *job)
{
    qDebug() << "Worker thread started for export job" << job->jobId;
    ExportJobSummary summary;
    summary.jobId = job->jobId;
    summary.totalItems = job->items.size();
    summary.destinationPath = job->destinationPath;

    qDebug() << "Executing export job" << job->jobId << "with"
             << job->items.size() << "items";

    for (int i = 0; i < job->items.size(); ++i) {
        // todo:Check for cancellation
        // if (job->cancelRequested.load() ||
        //     balloon->isCancelRequested(
        //         job->statusBalloonProcessId)) { // Use
        //                                         // statusBalloonProcessId
        //     summary.wasCancelled = true;
        //     qDebug() << "Export job" << job->jobId << "was cancelled";

        //     emit exportCancelled(job->jobId);
        //     return;
        // }

        const ExportItem &item = job->items.at(i);

        // emit exportProgress(job->jobId, i + 1, job->items.size(),
        //                     item.suggestedFileName);

        ExportResult result =
            exportSingleItem(item, job->destinationPath, job->altAfc,
                             job->cancelRequested, job->statusBalloonProcessId);
        if (result.success) {
            summary.successfulItems++;
            summary.totalBytesTransferred += result.bytesTransferred;
        } else {
            summary.failedItems++;
        }

        emit itemExported(job->statusBalloonProcessId, result);

        // // Check for cancellation again after potentially long file
        // // operation
        // if (job->cancelRequested.load() ||
        //     balloon->isCancelRequested(
        //         job->statusBalloonProcessId)) { // Use
        //                                         // statusBalloonProcessId
        //     summary.wasCancelled = true;
        //     qDebug() << "Export job" << job->jobId
        //              << "was cancelled during execution";

        //     QMetaObject::invokeMethod(
        //         QCoreApplication::instance(),
        //         [balloon,2
        //          id =
        //              job->statusBalloonProcessId]() { // Use
        //                                               //
        //                                               statusBalloonProcessId
        //             balloon->markProcessCancelled(id);
        //         },
        //         Qt::QueuedConnection);

        //     emit exportCancelled(job->jobId);
        //     return;
        // }
    }

    qDebug() << "Export job" << job->jobId
             << "completed - Success:" << summary.successfulItems
             << "Failed:" << summary.failedItems
             << "Bytes:" << summary.totalBytesTransferred;

    emit exportFinished(job->jobId, summary);
}

ExportResult ExportManagerThread::exportSingleItem(
    const ExportItem &item, const QString &destinationDir,
    std::optional<AfcClientHandle *> altAfc, std::atomic<bool> &cancelRequested,
    const QUuid &statusBalloonProcessId) // Change parameter name and type
{
    ExportResult result;
    result.sourceFilePath = item.sourcePathOnDevice;

    // Generate output path
    QString outputPath = QDir(destinationDir).filePath(item.suggestedFileName);
    // todo problem
    outputPath = generateUniqueOutputPath(outputPath);
    result.outputFilePath = outputPath;

    // Progress callback
    const QString &currentFile = item.suggestedFileName;
    int fileIndex = item.itemIndex;
    auto progressCallback =
        [this, statusBalloonProcessId, fileIndex,
         currentFile](qint64 transferred, // Use statusBalloonProcessId
                      qint64 total) {
            qDebug() << "Export progress callback for" << fileIndex
                     << "- transferred:" << transferred << "total:" << total;
            emit fileTransferProgress(statusBalloonProcessId, fileIndex,
                                      currentFile, transferred, total);
        };

    qDebug() << "About to export file from device:" << item.sourcePathOnDevice
             << "to" << outputPath;

    iDescriptorDevice *device =
        AppContext::sharedInstance()->getDevice(item.d_udid);

    // FIXME: is this way we do it?
    if (!device) {
        result.errorMessage = QString("Device with UDID %1 not found")
                                  .arg(QString::fromStdString(item.d_udid));
        qDebug() << result.errorMessage;
        return result;
    }

    // Export file using ServiceManager
    IdeviceFfiError *err = ServiceManager::exportFileToPath(
        device, item.sourcePathOnDevice.toUtf8().constData(),
        outputPath.toUtf8().constData(), progressCallback, &cancelRequested);

    if (err != nullptr) {
        result.errorMessage =
            QString("Failed to export file: %1").arg(err->message);
        qDebug() << result.errorMessage;
        idevice_error_free(err);
        return result;
    }

    // Get file size for statistics
    QFileInfo fileInfo(outputPath);
    result.bytesTransferred = fileInfo.size();
    result.success = true;

    return result;
}
QString ExportManagerThread::generateUniqueOutputPath(const QString &basePath)
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