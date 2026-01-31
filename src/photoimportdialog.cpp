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

#include "photoimportdialog.h"
#include "httpserver.h"
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>
#include <qrencode.h>

PhotoImportDialog::PhotoImportDialog(const QStringList &files,
                                     bool hasDirectories, QWidget *parent)
    : QDialog(parent), selectedFiles(files),
      containsDirectories(hasDirectories), m_httpServer(nullptr),
      m_mediaPlayer(nullptr)
{
    setupUI();
    setModal(true);
    resize(600, 700);
    setWindowTitle("Import Photos to iDevice - iDescriptor");
}

PhotoImportDialog::~PhotoImportDialog()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        delete m_mediaPlayer;
    }
    if (m_httpServer) {
        m_httpServer->stop();
        delete m_httpServer;
    }
}

void PhotoImportDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Warning label for directories
    if (containsDirectories) {
        warningLabel =
            new QLabel("⚠️ Warning: Selected items contain directories. All "
                       "gallery-compatible files will be included.",
                       this);
        warningLabel->setWordWrap(true);
        mainLayout->addWidget(warningLabel);
    }

    // File list
    QLabel *listLabel = new QLabel(
        QString("Files to be served (%1 items):").arg(selectedFiles.size()),
        this);
    mainLayout->addWidget(listLabel);

    fileList = new QListWidget(this);
    fileList->setMaximumHeight(150);
    for (const QString &file : selectedFiles) {
        QFileInfo info(file);
        fileList->addItem(info.fileName());
    }
    mainLayout->addWidget(fileList);

    // Horizontal layout for QR code and instructions
    QHBoxLayout *contentLayout = new QHBoxLayout();

    // QR Code area
    qrCodeLabel = new QLabel(this);
    qrCodeLabel->setAlignment(Qt::AlignCenter);
    qrCodeLabel->setMinimumSize(200, 200);
    qrCodeLabel->setMaximumSize(200, 200);
    qrCodeLabel->setText("QR Code will appear here after starting server");
    contentLayout->addWidget(qrCodeLabel);

    // Instructions container
    QVBoxLayout *instructionContainer = new QVBoxLayout();

    // Stacked widget for switchable instructions
    m_instructionStack = new QStackedWidget(this);
    m_instructionStack->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Expanding);

    // Text instructions
    m_instructionLabel = new QLabel("Loading", this);
    m_instructionLabel->setWordWrap(true);
    m_instructionStack->addWidget(m_instructionLabel);

    // Video instructions
    m_instructionVideo = new QVideoWidget(this);
    m_instructionVideo->setMinimumSize(300, 500);
    m_instructionVideo->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Expanding);
    m_mediaPlayer = new QMediaPlayer(this);
    m_mediaPlayer->setVideoOutput(m_instructionVideo);
    m_instructionStack->addWidget(m_instructionVideo);

    m_instructionVideo->setAspectRatioMode(
        Qt::AspectRatioMode::KeepAspectRatioByExpanding);
    m_instructionVideo->setStyleSheet(
        "QVideoWidget { background-color: transparent; }");

    instructionContainer->addWidget(m_instructionStack);

    // Toggle button
    m_toggleInstructionButton =
        new QPushButton("Show Video Instructions", this);
    connect(m_toggleInstructionButton, &QPushButton::clicked, this,
            &PhotoImportDialog::toggleInstructionMode);

    instructionContainer->addSpacing(10);

    QHBoxLayout *buttonContainer = new QHBoxLayout();
    buttonContainer->addStretch();
    buttonContainer->addWidget(m_toggleInstructionButton);
    buttonContainer->addStretch();
    instructionContainer->addLayout(buttonContainer);

    contentLayout->addLayout(instructionContainer);
    mainLayout->addLayout(contentLayout);

    // Progress tracking
    m_progressLabel = new QLabel("Download progress will appear here", this);
    m_progressLabel->setVisible(false);
    mainLayout->addWidget(m_progressLabel, Qt::AlignCenter);

    mainLayout->addSpacing(5);

    m_serverAddress = new QLabel("", this);
    m_serverAddress->setVisible(false);
    mainLayout->addWidget(m_serverAddress, Qt::AlignCenter);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_cancelButton = new QPushButton("Cancel", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // Setup video looping
    connect(m_mediaPlayer,
            QOverload<QMediaPlayer::MediaStatus>::of(
                &QMediaPlayer::mediaStatusChanged),
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) {
                    m_mediaPlayer->setPosition(0);
                    m_mediaPlayer->play();
                }
            });

    QTimer::singleShot(0, this, &PhotoImportDialog::init);
}

void PhotoImportDialog::init()
{

    // Create and start HTTP server
    m_httpServer = new HttpServer(this);
    connect(m_httpServer, &HttpServer::serverStarted, this,
            &PhotoImportDialog::onServerStarted);
    connect(m_httpServer, &HttpServer::serverError, this,
            &PhotoImportDialog::onServerError);
    connect(m_httpServer, &HttpServer::downloadProgress, this,
            &PhotoImportDialog::onDownloadProgress);

    m_httpServer->start(selectedFiles);
}

void PhotoImportDialog::onServerStarted()
{

    QString localIP = getLocalIP();
    int port = m_httpServer->getPort();
    QString jsonFileName = m_httpServer->getJsonFileName();
    QString url =
        QString("https://idescriptor.github.io/import?local=%1&port=%2&file=%3")
            .arg(localIP)
            .arg(port)
            .arg(jsonFileName);
    qDebug() << "Server url" << url;

    generateQRCode(url);

    m_instructionLabel->setText(
        "Instructions on How to Import\n\n1.Scan the QR code to open the "
        "web interface\n2.Click on \"Copy Server Address\"\n3.Click on "
        "\"Import and Run Shortcut\" if you have not installed the "
        "shortcut before or \"Run Shortcut\" if you have installed it "
        "before. \n4.Run the shortcut in the Shortcuts app. Once the "
        "shortcut imports to your device, it will automatically run "
        "\"Photos app\" \n\n Switch to video tutorial if you want to see a "
        "video tutorial.");

    m_mediaPlayer->setSource(
        QUrl("qrc:/resources/wireless-gallery-import.mp4"));

    m_progressLabel->setText("Waiting for downloads...");
    m_progressLabel->setVisible(true);

    m_serverAddress->setText(
        QString("Server started at %1:%2").arg(localIP).arg(port));
    m_serverAddress->setVisible(true);
}

void PhotoImportDialog::onDownloadProgress(const QString &fileName,
                                           int bytesDownloaded, int totalBytes)
{
    m_progressLabel->setText(QString("Downloaded: %1 (%2 KB)")
                                 .arg(fileName)
                                 .arg(bytesDownloaded / 1024));
}

void PhotoImportDialog::onServerError(const QString &error)
{
    m_cancelButton->setEnabled(true);

    QMessageBox::critical(this, "Server Error",
                          QString("Failed to start server: %1").arg(error));
    QDialog::reject();
}

void PhotoImportDialog::generateQRCode(const QString &url)
{
    QRcode *qrcode = QRcode_encodeString(url.toUtf8().constData(), 0,
                                         QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qrcode) {
        qrCodeLabel->setText("Failed to generate QR code");
        return;
    }

    int qrSize = 200;
    int qrWidth = qrcode->width;
    int scale = qrSize / qrWidth;
    if (scale < 1)
        scale = 1;

    QPixmap qrPixmap(qrWidth * scale, qrWidth * scale);
    qrPixmap.fill(Qt::white);

    QPainter painter(&qrPixmap);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int y = 0; y < qrWidth; y++) {
        for (int x = 0; x < qrWidth; x++) {
            if (qrcode->data[y * qrWidth + x] & 1) {
                QRect rect(x * scale, y * scale, scale, scale);
                painter.drawRect(rect);
            }
        }
    }

    qrCodeLabel->setPixmap(qrPixmap);
    QRcode_free(qrcode);
}

QString PhotoImportDialog::getLocalIP() const
{
    foreach (const QNetworkInterface &interface,
             QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            foreach (const QNetworkAddressEntry &entry,
                     interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    return entry.ip().toString();
                }
            }
        }
    }
    return "127.0.0.1";
}

void PhotoImportDialog::toggleInstructionMode()
{
    if (m_instructionStack->currentIndex() == 0) {
        // Switch to video
        m_instructionStack->setCurrentIndex(1);
        m_toggleInstructionButton->setText("Show Text Instructions");
        m_mediaPlayer->play();
    } else {
        // Switch to text
        m_instructionStack->setCurrentIndex(0);
        m_toggleInstructionButton->setText("Show Video Instructions");
        m_mediaPlayer->stop();
    }
}
