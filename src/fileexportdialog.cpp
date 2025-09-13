#include "fileexportdialog.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

// TODO: needs progress bar improvements
FileExportDialog::FileExportDialog(QWidget *parent)
    : QDialog(parent), m_progressBar(nullptr), m_statusLabel(nullptr),
      m_fileLabel(nullptr), m_cancelButton(nullptr), m_layout(nullptr)
{
    setupUI();
}

FileExportDialog::~FileExportDialog()
{
    // Qt handles cleanup automatically for child widgets
}

void FileExportDialog::setupUI()
{
    setWindowTitle("Exporting Files");
    setWindowModality(Qt::WindowModal);
    setFixedSize(400, 150);

    // Prevent user from closing dialog manually
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    m_layout = new QVBoxLayout(this);

    // Status label
    m_statusLabel = new QLabel("Preparing export...");
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    m_layout->addWidget(m_statusLabel);

    // File label
    m_fileLabel = new QLabel("");
    m_fileLabel->setStyleSheet("color: #666; font-size: 10px;");
    m_fileLabel->setWordWrap(true);
    m_layout->addWidget(m_fileLabel);

    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_layout->addWidget(m_progressBar);

    // Cancel button
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setStyleSheet("QPushButton { "
                                  "  background-color: #dc3545; "
                                  "  color: white; "
                                  "  border: none; "
                                  "  padding: 6px 12px; "
                                  "  border-radius: 3px; "
                                  "  font-weight: bold; "
                                  "} "
                                  "QPushButton:hover { "
                                  "  background-color: #c82333; "
                                  "} "
                                  "QPushButton:pressed { "
                                  "  background-color: #bd2130; "
                                  "}");

    connect(m_cancelButton, &QPushButton::clicked, this,
            [this]() { emit cancelRequested(); });

    buttonLayout->addWidget(m_cancelButton);
    m_layout->addLayout(buttonLayout);
}

void FileExportDialog::onExportStarted(int totalFiles)
{
    m_statusLabel->setText(
        QString("Starting export of %1 files...").arg(totalFiles));
    m_fileLabel->setText("Preparing...");
    m_progressBar->setValue(0);
    m_progressBar->setRange(0, 100);

    show();
    QApplication::processEvents();
}

void FileExportDialog::onExportProgress(int completed, int total,
                                        const QString &currentFileName)
{
    if (!isVisible()) {
        return;
    }

    int percentage = total > 0 ? (completed * 100) / total : 0;

    m_statusLabel->setText(
        QString("Exporting %1 of %2 files...").arg(completed).arg(total));
    m_fileLabel->setText(QString("Current file: %1").arg(currentFileName));
    m_progressBar->setValue(percentage);

    QApplication::processEvents();
}

void FileExportDialog::onExportFinished(int successful, int failed)
{
    // Hide the dialog first
    hide();

    // Show completion message
    showCompletionMessage(successful, failed);

    // Close and schedule for deletion
    close();
    deleteLater();
}

void FileExportDialog::onExportCancelled()
{
    // Hide the dialog first
    hide();

    // Show cancellation message
    QMessageBox::information(parentWidget(), "Export Cancelled",
                             "The export operation has been cancelled.");

    // Close and schedule for deletion
    close();
    deleteLater();
}

void FileExportDialog::showCompletionMessage(int successful, int failed)
{
    QString message;

    if (failed == 0) {
        message =
            QString("Successfully exported all %1 files!").arg(successful);
        QMessageBox::information(parentWidget(), "Export Complete", message);
    } else {
        message =
            QString("Export completed with %1 successful and %2 failed files.")
                .arg(successful)
                .arg(failed);
        QMessageBox::warning(parentWidget(), "Export Complete", message);
    }

    qDebug() << "Export finished:" << message;
}