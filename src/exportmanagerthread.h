#ifndef EXPORTMANAGERTHREAD_H
#define EXPORTMANAGERTHREAD_H
#include "iDescriptor.h"
#include "servicemanager.h"
#include <QDebug>
#include <QDir>
#include <QThread>

class ExportManager;

using namespace IdeviceFFI;

class ExportManagerThread : public QObject
{
    Q_OBJECT
public:
    ExportManagerThread(QObject *parent = nullptr) : QObject(parent) {}

    void executeExportJob(ExportJob *job);
    ExportResult exportSingleItem(const ExportItem &item,
                                  const QString &destinationDir,
                                  std::optional<AfcClientHandle *> altAfc,
                                  std::atomic<bool> &cancelRequested,
                                  const QUuid &statusBalloonProcessId);

private:
    void executeExportJobInternal(ExportJob *job);
    QString generateUniqueOutputPath(const QString &basePath);
signals:
    void exportProgress(const QUuid &jobId, int currentItem, int totalItems,
                        const QString &currentFileName);
    void fileTransferProgress(const QUuid &jobId, int fileIndex,
                              const QString &currentFile,
                              qint64 bytesTransferred, qint64 totalFileSize);
    void itemExported(const QUuid &jobId, const ExportResult &result);
    void exportFinished(const QUuid &jobId, const ExportJobSummary &summary);
    void exportCancelled(const QUuid &jobId);
};
#endif // EXPORTMANAGERTHREAD_H
