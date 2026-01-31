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

#ifndef PHOTOIMPORTDIALOG_H
#define PHOTOIMPORTDIALOG_H

#include "httpserver.h"
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QVideoWidget>
#include <QMediaPlayer>

class PhotoImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PhotoImportDialog(const QStringList &files, bool hasDirectories,
                               QWidget *parent = nullptr);
    ~PhotoImportDialog();

private slots:
    void init();
    void onServerStarted();
    void onServerError(const QString &error);
    void onDownloadProgress(const QString &fileName, int bytesDownloaded,
                            int totalBytes);
    void toggleInstructionMode();

private:
    QStringList selectedFiles;
    bool containsDirectories;

    QListWidget *fileList;
    QLabel *warningLabel;
    QLabel *qrCodeLabel;
    QStackedWidget *m_instructionStack;
    QLabel *m_instructionLabel;
    QVideoWidget *m_instructionVideo;
    QMediaPlayer *m_mediaPlayer;
    QPushButton *m_toggleInstructionButton;
    QPushButton *m_cancelButton;
    QLabel *m_progressLabel;
    QLabel *m_serverAddress;

    HttpServer *m_httpServer;

    void setupUI();
    void generateQRCode(const QString &url);
    QString getLocalIP() const;
};

#endif // PHOTOIMPORTDIALOG_H
