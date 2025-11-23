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

#include "appdownloadbasedialog.h"
#include "appstoremanager.h"
#include <QDesktopServices>
#include <QDir>
#include <QFutureWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

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
                                             const QString &bundleId,
                                             QWidget *parent)
    : QDialog(parent), m_appName(appName), m_downloadProcess(nullptr),
      m_progressTimer(nullptr), m_bundleId(bundleId)
{
    // Common UI: progress bar and action button
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(20);
    m_layout->setContentsMargins(30, 30, 30, 30);

    QLabel *nameLabel = new QLabel(appName);
    nameLabel->setStyleSheet("font-size: 20px; font-weight: bold;");
    m_layout->addWidget(nameLabel);

    m_actionButton = nullptr; // Derived classes set this
}

void AppDownloadBaseDialog::startDownloadProcess(const QString &bundleId,
                                                 const QString &outputDir,
                                                 int index,
                                                 bool promptToOpenDir)
{

    if (bundleId.isEmpty()) {
        QMessageBox::critical(this, "Error", "Bundle ID not provided.");
        reject();
        return;
    }

    addProgressBar(index);
    if (m_actionButton)
        m_actionButton->setEnabled(false);

    m_operationInProgress = true;
    tryToDownload(bundleId, outputDir, promptToOpenDir);
}

void AppDownloadBaseDialog::tryToDownload(const QString &bundleId,
                                          const QString &outputDir,
                                          bool promptToOpenDir)
{
    AppStoreManager *manager = AppStoreManager::sharedInstance();
    if (!manager) {
        QMessageBox::critical(this, "Error",
                              "Failed to initialize App Store manager.");
        reject();
        return;
    }
    bool acquireLicense = true;
    m_operationInProgress = true;
    QPointer<AppDownloadBaseDialog> safeThis = this;

    auto progressCallback = [safeThis](long long current, long long total) {
        if (!safeThis) {
            return;
        }
        int percentage = 0;
        if (total > 0) {
            percentage = static_cast<int>((current * 100) / total);
        }
        safeThis->updateProgressBar(percentage);
    };

    manager->downloadApp(
        bundleId, outputDir, "", acquireLicense,
        [safeThis, promptToOpenDir, outputDir](int result) {
            if (!safeThis) {
                return;
            }
            safeThis->m_operationInProgress = false;
            if (result == 0) { // Success
                emit safeThis->downloadFinished(true, "Success");
                if (safeThis->m_progressBar)
                    safeThis->m_progressBar->setValue(100);
                if (promptToOpenDir) {

                    if (QMessageBox::Yes ==
                        QMessageBox::question(
                            safeThis, "Download Successful",
                            QString("Successfully downloaded. Would you like "
                                    "to open the output directory: %1?")
                                .arg(outputDir))) {
                        QDir dir(outputDir);
                        if (!dir.exists()) {
                            QMessageBox::warning(
                                safeThis, "Directory Not Found",
                                QString("The directory %1 does not exist.")
                                    .arg(outputDir));
                        } else {
                            QDesktopServices::openUrl(
                                QUrl::fromLocalFile(outputDir));
                        }
                    }
                }
                safeThis->accept();
            } else { // Failure
                // 3 attempts
                if (safeThis->m_tries < 3) {
                    safeThis->m_tries++;
                    qDebug()
                        << "Retrying download for" + safeThis->m_bundleId +
                               "Attempt:" + QString::number(safeThis->m_tries);
                    safeThis->tryToDownload(safeThis->m_bundleId, outputDir,
                                            promptToOpenDir);
                    return;
                }
                emit safeThis->downloadFinished(false, "Failed");
                // if (promptToOpenDir)
                QMessageBox::critical(
                    safeThis, "Download Failed",
                    QString("Failed to download %1. Try signing out and back "
                            "in. Error code: %2")
                        .arg(safeThis->m_appName)
                        .arg(result));
                safeThis->reject();
            }
        },
        progressCallback);
}

void AppDownloadBaseDialog::reject()
{
    if (m_operationInProgress) {
        AppStoreManager *manager = AppStoreManager::sharedInstance();
        if (manager) {
            manager->cancelDownload(m_bundleId);
        }
        m_operationInProgress = false;
    }

    cleanup();
    QDialog::reject();
}

void AppDownloadBaseDialog::cleanup()
{
    if (m_progressTimer && m_progressTimer->isActive()) {
        m_progressTimer->stop();
    }
}