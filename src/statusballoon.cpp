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
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QTimerEvent>
#include <QUrl>
#include <QUuid>
#include <qpainterpath.h>

#ifdef WIN32
#include "platform/windows/win_common.h"
#endif

BalloonProcess::BalloonProcess(ProcessItem *item, QWidget *parent)
    : QWidget(parent), m_item(item)
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(15, 15, 15, 15);

    m_lastBytesTransferred = 0;
    m_lastUpdateTime = QDateTime::currentDateTime();

    // Title
    item->titleLabel = new QLabel(m_item->title);
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
        connect(item->actionButton, &QPushButton::clicked,
                StatusBalloon::sharedInstance(),
                &StatusBalloon::onOpenFolderClicked);
    }
    buttonsLayout->addWidget(item->actionButton);

    buttonsLayout->addStretch();

    // Cancel button
    item->cancelButton = new QPushButton("Cancel");
    connect(item->cancelButton, &QPushButton::clicked,
            StatusBalloon::sharedInstance(), &StatusBalloon::onCancelClicked);
    buttonsLayout->addWidget(item->cancelButton);

    layout->addLayout(buttonsLayout);
}

void BalloonProcess::setProgress(int progress)
{
    m_item->progressBar->setValue(progress);
}

void BalloonProcess::updateStats()
{
    QString statsText = QString("%1 of %2 items")
                            .arg(m_item->completedItems)
                            .arg(m_item->totalItems);
    if (m_item->failedItems > 0) {
        statsText += QString(" • %1 failed").arg(m_item->failedItems);
    }

    if (m_item->status == ProcessStatus::Running &&
        m_item->transferredBytes > 0) {
        // Calculate transfer rate
        QDateTime now = QDateTime::currentDateTime();
        qint64 elapsed = m_lastUpdateTime.msecsTo(now);
        if (elapsed > 0) {
            qint64 bytesDiff =
                m_item->transferredBytes - m_lastBytesTransferred;
            qint64 bytesPerSecond = (bytesDiff * 1000) / elapsed;
            if (bytesPerSecond > 0) {
                statsText += " • " + formatTransferRate(bytesPerSecond);
            }
            m_lastBytesTransferred = m_item->transferredBytes;
            m_lastUpdateTime = now;
        }
    }

    m_item->statsLabel->setText(statsText);
}

void BalloonProcess::updateButtons()
{
    // Update buttons
    if (m_item->status == ProcessStatus::Running) {
        m_item->cancelButton->setVisible(true);
        m_item->actionButton->setVisible(false);
    } else {
        m_item->cancelButton->setVisible(false);
        if (m_item->type == ProcessType::Export &&
            m_item->status == ProcessStatus::Completed) {
            m_item->actionButton->setVisible(true);
        }
    }
}

StatusBalloon *StatusBalloon::sharedInstance()
{
    static StatusBalloon instance;
    return &instance;
}

StatusBalloon::StatusBalloon(QWidget *parent) : QBalloonTip(parent)
{
    setMinimumHeight(300);
    setMinimumWidth(300);
#ifdef WIN32
    setAttribute(Qt::WA_TranslucentBackground);
#endif
    m_mainLayout = new QVBoxLayout();
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);

    m_noProcesesLabel =
        new QLabel("Export & Import processes will appear here", this);

    // Header label
    m_headerLabel = new QLabel("Processes");
    m_headerLabel->hide();
    QFont headerFont = m_headerLabel->font();
    headerFont.setPointSize(headerFont.pointSize() + 2);
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    m_mainLayout->addWidget(m_headerLabel);

    // Container for processes
    m_processesContainer = new QWidget();
    m_processesLayout = new QVBoxLayout(m_processesContainer);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidget(m_processesContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }");
    scrollArea->viewport()->setStyleSheet("background: transparent;");

    m_processesLayout->setSpacing(12);
    m_processesLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->addWidget(scrollArea);

    setLayout(m_mainLayout);
    connect(m_button, &ZIconWidget::clicked, this, &StatusBalloon::handleShow);
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
    // QTimer::singleShot(3000, this, [this]() {
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
    qDebug() << "StatusBalloon::updateProcessProgress";
    // QMutexLocker locker(&m_processesMutex);

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

    if (!item->processWidget) {
        qDebug()
            << "StatusBalloon::updateProcessProgress: no widget for processId"
            << processId;
        return;
    }

    handleJobUpdate(item);
}

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

    updateHeader();
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

    if (item->completedItems + item->failedItems == item->totalItems) {
        // meaning all items are processed, but we don't know if the overall
        // status is
        if (item->failedItems > 0) {
            item->status = ProcessStatus::Failed;
        } else {
            item->status = ProcessStatus::Completed;
        }
    }
    handleJobUpdate(item);
    updateHeader();
}

QUuid StatusBalloon::startExportProcess(const QString &title, int totalItems,
                                        const QString &destinationPath)
{
    qDebug() << "StatusBalloon::startExportProcess entry:" << title
             << totalItems << destinationPath;

    handleShow(); // ensure balloon is visible when process starts

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
    } // mutex released here

    // UI work must run without holding m_processesMutex to avoid re-locking
    // deadlock
    createProcessWidget(item);
    updateHeader();

    return item->processId;
}

void StatusBalloon::createProcessWidget(ProcessItem *item)
{
    BalloonProcess *processWidget = new BalloonProcess(item);
    item->processWidget = processWidget;
    m_processesLayout->addWidget(item->processWidget);
}

void StatusBalloon::updateHeader()
{
    // QMutexLocker locker(&m_processesMutex);

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
    int total = running + completed + failed;

    QString headerText = QString("Processes: %1 running").arg(running);
    if (completed > 0 || failed > 0) {
        headerText += QString(" • %1 completed").arg(completed);
        if (failed > 0) {
            headerText += QString(" • %1 failed").arg(failed);
        }
    }
    m_headerLabel->setText(headerText);

    if (total == 0) {
        m_headerLabel->hide();
        m_noProcesesLabel->show();
        return;
    } else {
        m_headerLabel->show();
        m_noProcesesLabel->hide();
    }
}

void StatusBalloon::handleShow(bool forceVisible)
{
    QPoint buttonBottomCenter = m_button->mapToGlobal(
        QPoint(m_button->width() / 2, m_button->height()));

    toggleBaloon(buttonBottomCenter, -1, forceVisible);
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

void StatusBalloon::handleJobUpdate(ProcessItem *item)
{
    // QMutexLocker locker(&m_processesMutex);

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
        item->processWidget->setProgress(progress);
    }

    item->processWidget->updateStats();
    item->processWidget->updateButtons();
}

#ifdef WIN32
void StatusBalloon::showEvent(QShowEvent *event)
{
    QBalloonTip::showEvent(event);
    // HWND changes after hide/show, so have reapply acrylic here
    enableMica((HWND)winId());
    SetCorner((HWND)winId(), CornerPreference::Corner_Round);
}
#endif

void StatusBalloon::resizeEvent(QResizeEvent *event)
{
    QBalloonTip::resizeEvent(event);

    if (!m_noProcesesLabel)
        return;

    m_noProcesesLabel->adjustSize();
    int x = (width() - m_noProcesesLabel->width()) / 2;
    int y = (height() - m_noProcesesLabel->height()) / 2;
    m_noProcesesLabel->move(x, y);
}
