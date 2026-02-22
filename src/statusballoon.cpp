#include "statusballoon.h"
#include "exportmanager.h"
#include "exportmanagerthread.h"
#include "iDescriptor.h"
#include "qballoontip.h"
#include <QApplication>
#include <QBasicTimer>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QTimerEvent>
#include <QUrl>
#include <QUuid>
#include <qpainterpath.h>

#ifdef WIN32
#include "platform/windows/win_common.h"
#endif

Process::Process(QWidget *parent) : QWidget(parent) {}

StatusBalloon *StatusBalloon::sharedInstance()
{
    static StatusBalloon instance;
    return &instance;
}

StatusBalloon::StatusBalloon(QWidget *parent)
    : QBalloonTip(QIcon(), "", "", parent)
{
    setMinimumHeight(300);
    setMinimumWidth(300);
#ifdef WIN32
    // FIXME: doesnt work the second time we call it
    enableAcrylic((HWND)winId());
#endif
    // Create main layout
    m_mainLayout = new QVBoxLayout();
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);

    // Header label
    m_headerLabel = new QLabel("Processes");
    QFont headerFont = m_headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    m_mainLayout->addWidget(m_headerLabel);

    // Container for processes
    m_processesContainer = new QWidget();
    m_processesLayout = new QVBoxLayout(m_processesContainer);
    m_processesLayout->setSpacing(12);
    m_processesLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_processesContainer);

    setLayout(m_mainLayout);
    connect(m_button, &ZIconWidget::clicked, this, &StatusBalloon::showBalloon);
    connectExportThreadSignals();
}

void StatusBalloon::connectExportThreadSignals()
{
    ExportManager *exportManager = ExportManager::sharedInstance();

    connect(exportManager->m_exportThread, &ExportManagerThread::exportFinished,
            this, &StatusBalloon::onExportFinished);

    connect(exportManager->m_exportThread, &ExportManagerThread::itemExported,
            this, &StatusBalloon::onItemExported);

    connect(exportManager->m_exportThread,
            &ExportManagerThread::fileTransferProgress, this,
            &StatusBalloon::onFileTransferProgress);
    // QTimer::singleShot(0, this, [this]() {
    //     // test
    //     startExportProcess("Test Export Process", 10,
    //     "/path/to/destination");
    // });
}

void StatusBalloon::onFileTransferProgress(const QUuid &processId,
                                           int currentItem,
                                           const QString &currentFile,
                                           qint64 bytesTransferred,
                                           qint64 totalBytes)
{
    qDebug() << "StatusBalloon::updateProcessProgress entry:" << processId
             << currentItem << currentFile << bytesTransferred << totalBytes;
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        qDebug() << "StatusBalloon::updateProcessProgress: unknown processId"
                 << processId;
        return;
    }

    ProcessItem *item = m_processes[processId];
    item->completedItems = currentItem;
    item->currentFile = currentFile;
    item->transferredBytes = bytesTransferred;
    item->totalBytes = totalBytes;

    if (!item->processWidget)
        qDebug()
            << "StatusBalloon::updateProcessProgress: no widget for processId"
            << processId;

    // Update status label
    QString statusText;
    if (item->status == ProcessStatus::Running) {
        if (!item->currentFile.isEmpty()) {
            statusText = item->currentFile;
        } else {
            statusText = "Processing...";
        }
    } else if (item->status == ProcessStatus::Completed) {
        statusText = "Completed successfully";
    } else if (item->status == ProcessStatus::Failed) {
        statusText = "Failed";
    } else if (item->status == ProcessStatus::Cancelled) {
        statusText = "Cancelled";
    }
    item->statusLabel->setText(statusText);

    // Update progress bar
    // progess should be based on exported bytes vs total bytes of the current
    // file
    if (item->totalItems > 0) {
        int progress = (item->transferredBytes * 100) / item->totalBytes;
        item->progressBar->setValue(progress);
    }

    // Update stats
    QString statsText = QString("%1 of %2 items")
                            .arg(item->completedItems)
                            .arg(item->totalItems);
    if (item->failedItems > 0) {
        statsText += QString(" • %1 failed").arg(item->failedItems);
    }

    if (item->status == ProcessStatus::Running && item->transferredBytes > 0) {
        // Calculate transfer rate
        QDateTime now = QDateTime::currentDateTime();
        qint64 elapsed = m_lastUpdateTime[item->processId].msecsTo(now);
        if (elapsed > 0) {
            qint64 bytesDiff = item->transferredBytes -
                               m_lastBytesTransferred[item->processId];
            qint64 bytesPerSecond = (bytesDiff * 1000) / elapsed;
            if (bytesPerSecond > 0) {
                statsText += " • " + formatTransferRate(bytesPerSecond);
            }
            m_lastBytesTransferred[item->processId] = item->transferredBytes;
            m_lastUpdateTime[item->processId] = now;
        }
    }

    item->statsLabel->setText(statsText);

    // Update buttons
    if (item->status == ProcessStatus::Running) {
        item->cancelButton->setVisible(true);
        item->actionButton->setVisible(false);
    } else {
        item->cancelButton->setVisible(false);
        if (item->type == ProcessType::Export &&
            item->status == ProcessStatus::Completed) {
            item->actionButton->setVisible(true);
        }
    }
}

// todo fix these
// StatusBalloon::onItemExported entry:
// QUuid("{9bd97848-cb52-4ef8-93c1-a1d1c285e6a3}") Success: true
// StatusBalloon::onItemExported: unknown processId
// QUuid("{9bd97848-cb52-4ef8-93c1-a1d1c285e6a3}")
// StatusBalloon::onExportFinished entry:
// QUuid("{9bd97848-cb52-4ef8-93c1-a1d1c285e6a3}") WasCancelled: false
// StatusBalloon::onExportFinished: unknown processId
// QUuid("{9bd97848-cb52-4ef8-93c1-a1d1c285e6a3}")

void StatusBalloon::onExportFinished(const QUuid &processId,
                                     const ExportJobSummary &summary)
{
    qDebug() << "StatusBalloon::onExportFinished entry:" << processId
             << "WasCancelled:" << summary.wasCancelled;
    QMutexLocker locker(&m_processesMutex);
    if (!m_processes.contains(processId)) {
        qDebug() << "StatusBalloon::onExportFinished: unknown processId"
                 << processId;
        return;
    }

    // todo: handle failed ?
    ProcessItem *item = m_processes[processId];
    if (summary.wasCancelled) {
        item->status = ProcessStatus::Cancelled;
    } else {
        item->status = ProcessStatus::Completed;
    }
    item->endTime = QDateTime::currentDateTime();

    updateUI();
}

void StatusBalloon::onItemExported(const QUuid &processId,
                                   const ExportResult &result)
{
    qDebug() << "StatusBalloon::onItemExported entry:" << processId
             << "Success:" << result.success;
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        qDebug() << "StatusBalloon::onItemExported: unknown processId"
                 << processId;
        return;
    }

    ProcessItem *item = m_processes[processId];
    if (result.success) {
        item->completedItems += 1;
    } else {
        item->failedItems += 1;
    }

    updateUI();
}

QUuid StatusBalloon::startExportProcess(const QString &title, int totalItems,
                                        const QString &destinationPath)
{
    qDebug() << "StatusBalloon::startExportProcess entry:" << title
             << totalItems << destinationPath;

    // allocate item first so it can be used after unlocking
    auto *item = new ProcessItem();
    item->processId = QUuid::createUuid();
    item->type = ProcessType::Export;
    item->status = ProcessStatus::Running;
    item->title = title;
    item->totalItems = totalItems;
    item->completedItems = 0;
    item->failedItems = 0;
    item->totalBytes = 0;
    item->transferredBytes = 0;
    item->startTime = QDateTime::currentDateTime();
    item->destinationPath = destinationPath;

    { // scope the lock only for shared-state mutation
        QMutexLocker locker(&m_processesMutex);
        m_processes[item->processId] = item;
        m_currentProcessId = item->processId;
        m_lastBytesTransferred[item->processId] = 0;
        m_lastUpdateTime[item->processId] = QDateTime::currentDateTime();
    } // mutex released here

    // UI work must run without holding m_processesMutex to avoid re-locking
    // deadlock
    createProcessWidget(item);
    updateUI();

    return item->processId;
}

QUuid StatusBalloon::startUploadProcess(const QString &title, int totalItems)
{
    // allocate item first
    auto *item = new ProcessItem();
    item->processId = QUuid::createUuid();
    item->type = ProcessType::Upload;
    item->status = ProcessStatus::Running;
    item->title = title;
    item->totalItems = totalItems;
    item->completedItems = 0;
    item->failedItems = 0;
    item->totalBytes = 0;
    item->transferredBytes = 0;
    item->startTime = QDateTime::currentDateTime();

    { // scope the lock only for shared-state mutation
        QMutexLocker locker(&m_processesMutex);
        m_processes[item->processId] = item;
        m_currentProcessId = item->processId;
        m_lastBytesTransferred[item->processId] = 0;
        m_lastUpdateTime[item->processId] = QDateTime::currentDateTime();
    } // mutex released here

    createProcessWidget(item);
    updateUI();

    return item->processId;
}

void StatusBalloon::createProcessWidget(ProcessItem *item)
{
    item->processWidget = new QWidget();
    auto *layout = new QVBoxLayout(item->processWidget);
    layout->setSpacing(6);
    layout->setContentsMargins(0, 0, 0, 0);

    // Title
    item->titleLabel = new QLabel(item->title);
    QFont titleFont = item->titleLabel->font();
    titleFont.setBold(true);
    item->titleLabel->setFont(titleFont);
    layout->addWidget(item->titleLabel);

    // Status
    item->statusLabel = new QLabel("Starting...");
    layout->addWidget(item->statusLabel);

    // Progress bar
    item->progressBar = new QProgressBar();
    item->progressBar->setRange(0, 100);
    item->progressBar->setValue(0);
    item->progressBar->setTextVisible(true); // show text for debugging
    item->progressBar->setFixedHeight(12);   // make it visible
    layout->addWidget(item->progressBar);

    // Stats
    item->statsLabel = new QLabel();
    QFont statsFont = item->statsLabel->font();
    statsFont.setPointSize(statsFont.pointSize() - 1);
    item->statsLabel->setFont(statsFont);
    layout->addWidget(item->statsLabel);

    // Buttons layout
    auto *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(6);

    // Action button (Open Folder for export, hidden initially)
    item->actionButton = new QPushButton();
    item->actionButton->setVisible(false);
    if (item->type == ProcessType::Export) {
        item->actionButton->setText("Open Folder");
        connect(item->actionButton, &QPushButton::clicked, this,
                &StatusBalloon::onOpenFolderClicked);
    }
    buttonsLayout->addWidget(item->actionButton);

    buttonsLayout->addStretch();

    // Cancel button
    item->cancelButton = new QPushButton("Cancel");
    connect(item->cancelButton, &QPushButton::clicked, this,
            &StatusBalloon::onCancelClicked);
    buttonsLayout->addWidget(item->cancelButton);

    layout->addLayout(buttonsLayout);

    m_processesLayout->addWidget(item->processWidget);
}

void StatusBalloon::markProcessCompleted(const QUuid &processId)
{
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        return;
    }

    ProcessItem *item = m_processes[processId];
    item->status = ProcessStatus::Completed;
    item->endTime = QDateTime::currentDateTime();

    updateUI();

    // Check if all processes are done
    bool allDone = true;
    for (auto *proc : m_processes) {
        if (proc->status == ProcessStatus::Running) {
            allDone = false;
            break;
        }
    }
}

void StatusBalloon::markProcessFailed(const QUuid &processId,
                                      const QString &error)
{
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        return;
    }

    ProcessItem *item = m_processes[processId];
    item->status = ProcessStatus::Failed;
    item->endTime = QDateTime::currentDateTime();

    updateUI();
}

void StatusBalloon::markProcessCancelled(const QUuid &processId)
{
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        return;
    }

    ProcessItem *item = m_processes[processId];
    item->status = ProcessStatus::Cancelled;
    item->endTime = QDateTime::currentDateTime();

    updateUI();
}

void StatusBalloon::incrementFailedItems(const QUuid &processId)
{
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        return;
    }

    m_processes[processId]->failedItems++;
    updateUI();
}

void StatusBalloon::updateUI()
{
    QMutexLocker locker(&m_processesMutex);

    // Update header
    int running = 0, completed = 0, failed = 0;
    for (auto *item : m_processes) {
        if (item->status == ProcessStatus::Running)
            running++;
        else if (item->status == ProcessStatus::Completed)
            completed++;
        else if (item->status == ProcessStatus::Failed)
            failed++;
    }

    QString headerText = QString("Processes: %1 running").arg(running);
    if (completed > 0 || failed > 0) {
        headerText += QString(" • %1 completed").arg(completed);
        if (failed > 0) {
            headerText += QString(" • %1 failed").arg(failed);
        }
    }
    m_headerLabel->setText(headerText);

    // Update each process widget
    for (auto *item : m_processes) {
        if (!item->processWidget)
            continue;

        // Update status label
        QString statusText;
        if (item->status == ProcessStatus::Running) {
            if (!item->currentFile.isEmpty()) {
                statusText = item->currentFile;
            } else {
                statusText = "Processing...";
            }
        } else if (item->status == ProcessStatus::Completed) {
            statusText = "Completed successfully";
        } else if (item->status == ProcessStatus::Failed) {
            statusText = "Failed";
        } else if (item->status == ProcessStatus::Cancelled) {
            statusText = "Cancelled";
        }
        item->statusLabel->setText(statusText);

        // Update progress bar
        if (item->totalItems > 0) {
            int progress = (item->completedItems * 100) / item->totalItems;
            item->progressBar->setValue(progress);
        }

        // Update stats
        QString statsText = QString("%1 of %2 items")
                                .arg(item->completedItems)
                                .arg(item->totalItems);
        if (item->failedItems > 0) {
            statsText += QString(" • %1 failed").arg(item->failedItems);
        }

        if (item->status == ProcessStatus::Running &&
            item->transferredBytes > 0) {
            // Calculate transfer rate
            QDateTime now = QDateTime::currentDateTime();
            qint64 elapsed = m_lastUpdateTime[item->processId].msecsTo(now);
            if (elapsed > 0) {
                qint64 bytesDiff = item->transferredBytes -
                                   m_lastBytesTransferred[item->processId];
                qint64 bytesPerSecond = (bytesDiff * 1000) / elapsed;
                if (bytesPerSecond > 0) {
                    statsText += " • " + formatTransferRate(bytesPerSecond);
                }
                m_lastBytesTransferred[item->processId] =
                    item->transferredBytes;
                m_lastUpdateTime[item->processId] = now;
            }
        }

        item->statsLabel->setText(statsText);

        // Update buttons
        if (item->status == ProcessStatus::Running) {
            item->cancelButton->setVisible(true);
            item->actionButton->setVisible(false);
        } else {
            item->cancelButton->setVisible(false);
            if (item->type == ProcessType::Export &&
                item->status == ProcessStatus::Completed) {
                item->actionButton->setVisible(true);
            }
        }
    }

    showBalloon();
}

void StatusBalloon::showBalloon()
{
    qDebug() << "StatusBalloon::showBalloon" << sender();
    QPoint pos = m_button->mapToGlobal(
        QPoint(m_button->width() / 2, m_button->height()));

    balloon(pos, -1, true);
}

bool StatusBalloon::isProcessRunning(const QUuid &processId) const
{
    QMutexLocker locker(&m_processesMutex);
    if (!m_processes.contains(processId)) {
        return false;
    }
    return m_processes[processId]->status == ProcessStatus::Running;
}

bool StatusBalloon::hasActiveProcesses() const
{
    QMutexLocker locker(&m_processesMutex);
    for (auto *item : m_processes) {
        if (item->status == ProcessStatus::Running) {
            return true;
        }
    }
    return false;
}

bool StatusBalloon::isCancelRequested(const QUuid &processId) const
{
    QMutexLocker locker(&m_processesMutex);
    if (!m_processes.contains(processId)) {
        return false;
    }
    return m_processes[processId]->cancelRequested.load();
}

void StatusBalloon::onCancelClicked()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    QMutexLocker locker(&m_processesMutex);

    // Find which process this button belongs to
    for (auto *item : m_processes) {
        if (item->cancelButton == button) {
            item->cancelRequested.store(true);
            button->setEnabled(false);
            button->setText("Cancelling...");
            break;
        }
    }
}

void StatusBalloon::onOpenFolderClicked()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    QMutexLocker locker(&m_processesMutex);

    for (auto *item : m_processes) {
        if (item->actionButton == button && item->type == ProcessType::Export) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(item->destinationPath));
            break;
        }
    }
}

QString StatusBalloon::formatFileSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString("%1 GB").arg(
            QString::number(bytes / double(GB), 'f', 2));
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(
            QString::number(bytes / double(MB), 'f', 1));
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(
            QString::number(bytes / double(KB), 'f', 0));
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString StatusBalloon::formatTransferRate(qint64 bytesPerSecond) const
{
    return formatFileSize(bytesPerSecond) + "/s";
}

void StatusBalloon::removeProcessWidget(const QUuid &processId)
{
    QMutexLocker locker(&m_processesMutex);

    if (!m_processes.contains(processId)) {
        return;
    }

    ProcessItem *item = m_processes[processId];
    if (item->processWidget) {
        m_processesLayout->removeWidget(item->processWidget);
        item->processWidget->deleteLater();
    }

    // delete item;
    m_processes.remove(processId);

    if (m_processes.isEmpty()) {
        hide();
    }
}

ZIconWidget *StatusBalloon::getButton() { return m_button; }
