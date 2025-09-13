#include "photoexportmanager.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>

PhotoExportManager::PhotoExportManager(QObject *parent)
    : QObject(parent), m_device(nullptr), m_isExporting(false),
      m_cancelRequested(false), m_workerThread(nullptr)
{
}

void PhotoExportManager::exportFiles(iDescriptorDevice *device,
                                     const QStringList &filePaths,
                                     const QString &outputDirectory)
{
    QMutexLocker locker(&m_mutex);

    if (m_isExporting) {
        qWarning() << "Export operation already in progress";
        return;
    }

    if (!device || !device->afcClient) {
        qWarning() << "Invalid device or AFC client";
        return;
    }

    if (filePaths.isEmpty()) {
        qWarning() << "No files to export";
        return;
    }

    // Validate output directory
    QDir outputDir(outputDirectory);
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(".")) {
            qWarning() << "Could not create output directory:"
                       << outputDirectory;
            return;
        }
    }

    m_device = device;
    m_filePaths = filePaths;
    m_outputDirectory = outputDirectory;
    m_isExporting = true;
    m_cancelRequested = false;

    qDebug() << "Starting export of" << filePaths.size() << "files to"
             << outputDirectory;

    emit exportStarted(filePaths.size());

    // Start export in worker thread
    m_workerThread = QThread::create([this]() { performExport(); });

    // TODO: refactor to qfuture
    connect(m_workerThread, &QThread::finished, m_workerThread,
            &QThread::deleteLater);
    connect(m_workerThread, &QThread::finished, this, [this]() {
        QMutexLocker locker(&m_mutex);
        m_workerThread = nullptr;
        m_isExporting = false;
    });

    m_workerThread->start();
}

void PhotoExportManager::cancelExport()
{
    QMutexLocker locker(&m_mutex);
    if (m_isExporting) {
        m_cancelRequested = true;
        qDebug() << "Export cancellation requested";
    }
}

void PhotoExportManager::performExport()
{
    int successful = 0;
    int failed = 0;

    for (int i = 0; i < m_filePaths.size(); ++i) {
        // Check for cancellation
        {
            QMutexLocker locker(&m_mutex);
            if (m_cancelRequested) {
                qDebug() << "Export cancelled by user";
                emit exportCancelled();
                return;
            }
        }

        const QString &devicePath = m_filePaths.at(i);
        QString fileName = extractFileName(devicePath);

        QString outputPath = QDir(m_outputDirectory).filePath(fileName);

        // Generate unique path if file exists
        outputPath = generateUniqueOutputPath(outputPath);

        ExportResult result =
            exportSingleFile(m_device->afcClient, devicePath, outputPath);

        if (result.success) {
            successful++;
            qDebug() << "Successfully exported:" << fileName;
        } else {
            failed++;
            qWarning() << "Failed to export" << fileName << ":"
                       << result.errorMessage;
        }

        emit fileExported(result);
        emit exportProgress(i + 1, m_filePaths.size(), fileName);
    }

    // hideProgressDialog();

    qDebug() << "Export completed - Success:" << successful
             << "Failed:" << failed;
    emit exportFinished(successful, failed);
}

PhotoExportManager::ExportResult PhotoExportManager::exportSingleFile(
    afc_client_t afc, const QString &devicePath, const QString &outputPath)
{
    ExportResult result;
    result.filePath = devicePath;
    result.outputPath = outputPath;
    result.success = false;

    // Open file on device
    uint64_t handle = 0;
    afc_error_t afc_err = afc_file_open(afc, devicePath.toUtf8().constData(),
                                        AFC_FOPEN_RDONLY, &handle);

    if (afc_err != AFC_E_SUCCESS) {
        result.errorMessage =
            QString("Failed to open file on device: %1 (AFC error: %2)")
                .arg(devicePath)
                .arg(static_cast<int>(afc_err));
        return result;
    }

    // Open local output file
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        result.errorMessage = QString("Failed to create local file: %1 (%2)")
                                  .arg(outputPath)
                                  .arg(outputFile.errorString());
        afc_file_close(afc, handle);
        return result;
    }

    // Copy data from device to local file
    char buffer[4096];
    uint32_t bytesRead = 0;
    qint64 totalBytes = 0;

    while (afc_file_read(afc, handle, buffer, sizeof(buffer), &bytesRead) ==
               AFC_E_SUCCESS &&
           bytesRead > 0) {
        // Check for cancellation during file copy
        {
            QMutexLocker locker(&m_mutex);
            if (m_cancelRequested) {
                outputFile.close();
                outputFile.remove(); // Clean up partial file
                afc_file_close(afc, handle);
                result.errorMessage = "Export cancelled";
                return result;
            }
        }

        qint64 bytesWritten = outputFile.write(buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            result.errorMessage =
                QString("Write error: only wrote %1 of %2 bytes")
                    .arg(bytesWritten)
                    .arg(bytesRead);
            outputFile.close();
            outputFile.remove(); // Clean up partial file
            afc_file_close(afc, handle);
            return result;
        }

        totalBytes += bytesRead;
    }

    // Clean up
    outputFile.close();
    afc_file_close(afc, handle);

    if (totalBytes == 0) {
        result.errorMessage = "No data read from device file";
        outputFile.remove(); // Clean up empty file
        return result;
    }

    result.success = true;
    qDebug() << "Exported" << totalBytes << "bytes from" << devicePath << "to"
             << outputPath;
    return result;
}

QString PhotoExportManager::extractFileName(const QString &devicePath) const
{
    // Extract filename from device path (similar to strrchr in C)
    int lastSlash = devicePath.lastIndexOf('/');
    if (lastSlash != -1 && lastSlash < devicePath.length() - 1) {
        return devicePath.mid(lastSlash + 1);
    }
    return devicePath; // Return full path if no slash found
}

QString
PhotoExportManager::generateUniqueOutputPath(const QString &basePath) const
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
    } while (QFile::exists(uniquePath) &&
             counter < 10000); // Prevent infinite loop

    return uniquePath;
}