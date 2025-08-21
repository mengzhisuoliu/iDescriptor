#include "appdownloadbasedialog.h"
#include "libipatool-go.h"
#include <QDesktopServices>
#include <QDir>
#include <QFutureWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

void downloadProgressCallback(long long current, long long total,
                              void *userData)
{
    // Cast the user data back to the dialog instance.
    AppDownloadBaseDialog *dialog =
        static_cast<AppDownloadBaseDialog *>(userData);
    if (dialog) {
        int percentage = 0;
        if (total > 0) {
            percentage = static_cast<int>((current * 100) / total);
        }
        // Safely call the update method on the GUI thread.
        QMetaObject::invokeMethod(dialog, "updateProgressBar",
                                  Qt::QueuedConnection, Q_ARG(int, percentage));
    }
}

void AppDownloadBaseDialog::updateProgressBar(int percentage)
{

    if (m_progressBar) {
        m_progressBar->setValue(percentage);
    }
}

void AppDownloadBaseDialog::addProgressBar(int index)
{
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(25);
    m_progressBar->setStyleSheet(
        "QProgressBar { border-radius: 6px; background: #eee; } "
        "QProgressBar::chunk { background: #34C759; }");
    m_layout->insertWidget(index, m_progressBar);
}

AppDownloadBaseDialog::AppDownloadBaseDialog(const QString &appName,
                                             QWidget *parent)
    : QDialog(parent), m_appName(appName), m_downloadProcess(nullptr),
      m_progressTimer(nullptr)
{
    // Common UI: progress bar and action button
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(20);
    m_layout->setContentsMargins(30, 30, 30, 30);

    QLabel *nameLabel = new QLabel(appName);
    nameLabel->setStyleSheet(
        "font-size: 20px; font-weight: bold; color: #333;");
    m_layout->addWidget(nameLabel);

    m_actionButton = nullptr; // Derived classes set this
}

void AppDownloadBaseDialog::startDownloadProcess(const QString &bundleId,
                                                 const QString &outputDir,
                                                 int index)
{
    bool acquireLicense = true;

    if (bundleId.isEmpty()) {
        QMessageBox::critical(this, "Error", "Bundle ID not provided.");
        reject();
        return;
    }

    addProgressBar(index);
    if (m_actionButton)
        m_actionButton->setEnabled(false);

    // // C-style callback function for progress updates from the Go library
    // auto progressCallback = [this](long long current, long long total) {

    //     // Use invokeMethod to call a slot on the GUI thread safely
    //     // QMetaObject::invokeMethod(this, "updateProgressBar",
    //     //                           Qt::QueuedConnection, Q_ARG(int,
    //     //                           percentage));
    //     updateProgressBar(percentage);
    // };

    QFuture<int> future =
        QtConcurrent::run([bundleId, outputDir, acquireLicense, this]() {
            // Call the Go function directly
            return IpaToolDownloadApp(
                bundleId.toUtf8().data(), outputDir.toUtf8().data(), "",
                acquireLicense, downloadProgressCallback, this);
        });

    QFutureWatcher<int> *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this,
            [this, watcher, outputDir]() {
                int result = watcher->result();
                watcher->deleteLater();

                if (result == 0) { // Success
                    m_progressBar->setValue(100);
                    if (QMessageBox::Yes ==
                        QMessageBox::question(
                            this, "Download Successful",
                            QString("Successfully downloaded. Would you like "
                                    "to open the output directory: %1?")
                                .arg(outputDir))) {
                        QDir dir(outputDir);
                        if (!dir.exists()) {
                            QMessageBox::warning(
                                this, "Directory Not Found",
                                QString("The directory %1 does not exist.")
                                    .arg(outputDir));
                        } else {
                            QDesktopServices::openUrl(
                                QUrl::fromLocalFile(outputDir));
                        }
                    }
                    accept();
                } else { // Failure
                    QMessageBox::critical(
                        this, "Download Failed",
                        QString("Failed to download %1. Error code: %2")
                            .arg(m_appName)
                            .arg(result));
                    reject();
                }
            });
    watcher->setFuture(future);
}