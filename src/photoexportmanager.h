#ifndef PHOTOEXPORTMANAGER_H
#define PHOTOEXPORTMANAGER_H

#include "iDescriptor.h"
#include <QFileInfo>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>

class PhotoExportManager : public QObject
{
    Q_OBJECT

public:
    explicit PhotoExportManager(QObject *parent = nullptr);

    struct ExportResult {
        QString filePath;
        QString outputPath;
        bool success;
        QString errorMessage;
    };

    // Start export operation
    void exportFiles(iDescriptorDevice *device, const QStringList &filePaths,
                     const QString &outputDirectory);

    // Check if export is currently running
    bool isExporting() const { return m_isExporting; }

    // Cancel current export operation
    void cancelExport();

signals:
    void exportStarted(int totalFiles);
    void fileExported(const PhotoExportManager::ExportResult &result);
    void exportProgress(int completed, int total,
                        const QString &currentFileName);
    void exportFinished(int successful, int failed);
    void exportCancelled();

private slots:
    void performExport();

private:
    // Export single file using AFC
    ExportResult exportSingleFile(afc_client_t afc, const QString &devicePath,
                                  const QString &outputPath);

    // Extract filename from device path
    QString extractFileName(const QString &devicePath) const;

    // Generate unique output path if file exists
    QString generateUniqueOutputPath(const QString &basePath) const;

    // Member variables
    iDescriptorDevice *m_device;
    QStringList m_filePaths;
    QString m_outputDirectory;
    bool m_isExporting;
    bool m_cancelRequested;
    QMutex m_mutex;
    QThread *m_workerThread;
};

#endif // PHOTOEXPORTMANAGER_H