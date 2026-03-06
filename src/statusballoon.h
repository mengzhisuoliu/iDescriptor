#ifndef STATUSBALLOON_H
#define STATUSBALLOON_H

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "qballoontip.h"
#include <QBasicTimer>
#include <QDateTime>
#include <QIcon>
#include <QLabel>
#include <QMutex>
#include <QProgressBar>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <atomic>
class BalloonProcess;

enum class ProcessType { Export, Upload };

enum class ProcessStatus { Queued, Running, Completed, Failed, Cancelled };

struct ProcessItem {
    QUuid processId;
    ProcessType type;
    ProcessStatus status;
    QString title;
    QString currentFile;
    int totalItems;
    int completedItems;
    int failedItems;
    qint64 totalBytes;
    qint64 transferredBytes;
    QDateTime startTime;
    QDateTime endTime;
    QString destinationPath; // For export
    BalloonProcess *processWidget;
    QLabel *titleLabel;
    QLabel *statusLabel;
    QLabel *statsLabel;
    QProgressBar *progressBar;
    QPushButton *actionButton;
    QPushButton *cancelButton;
    std::atomic<bool> cancelRequested{false};
};

class BalloonProcess : public QWidget
{
    Q_OBJECT
public:
    explicit BalloonProcess(ProcessItem *item, QWidget *parent = nullptr);

    void setProgress(int progress);
    void updateStats();
    void updateButtons();
    void done();

private:
    ProcessItem *m_item;
    QDateTime m_lastUpdateTime;
    qint64 m_lastBytesTransferred;
};

class StatusBalloon : public QBalloonTip
{
    Q_OBJECT
public:
    explicit StatusBalloon(QWidget *parent = nullptr);
    static StatusBalloon *sharedInstance();

    // Process management
    QUuid startExportProcess(const QString &title, int totalItems,
                             const QString &destinationPath);

    void onFileTransferProgress(const QUuid &processId, int currentItem,
                                const QString &currentFile,
                                qint64 bytesTransferred, qint64 totalBytes);

    bool isProcessRunning(const QUuid &processId) const;
    bool hasActiveProcesses() const;
    bool isCancelRequested(const QUuid &processId) const;
    void onCancelClicked();
    void onOpenFolderClicked();

protected:
#ifdef WIN32
    void showEvent(QShowEvent *event) override;
// void paintEvent(QPaintEvent *event) override;
#endif
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateHeader();
    void handleShow(bool forceVisible = false);
    void createProcessWidget(ProcessItem *item);
    void removeProcessWidget(const QUuid &processId);
    void connectExportThreadSignals();
    void onExportFinished(const QUuid &processId,
                          const ExportJobSummary &summary);
    void onItemExported(const QUuid &processId, const ExportResult &result);
    void handleJobUpdate(ProcessItem *item);

    QVBoxLayout *m_mainLayout;
    QLabel *m_headerLabel;
    QWidget *m_processesContainer;
    QVBoxLayout *m_processesLayout;

    QMap<QUuid, ProcessItem *> m_processes;
    QUuid m_currentProcessId;
    mutable QMutex m_processesMutex;
    QLabel *m_noProcesesLabel;
};
#endif // STATUSBALLOON_H
