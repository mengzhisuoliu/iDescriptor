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

#include "httpserver.h"
#include "iDescriptor.h"
#include "settingsmanager.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QUrl>

HttpServer::HttpServer(QObject *parent)
    : QObject(parent), server(new QTcpServer(this)), port(8080)
{
    connect(server, &QTcpServer::newConnection, this,
            &HttpServer::onNewConnection);
}

HttpServer::~HttpServer() { stop(); }

void HttpServer::start(const QStringList &files)
{
    fileList = files;

    // Generate unique JSON filename
    QString timestamp =
        QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    jsonFileName = QString("%1-idescriptor-import.json").arg(timestamp);

    // Try to bind to port from settings, if fails try other ports
    int startPort = SettingsManager::sharedInstance()->wirelessFileServerPort();
    qDebug() << "Starting HTTP server on port" << startPort;
    for (int tryPort = startPort; tryPort <= startPort + 10; ++tryPort) {
        if (server->listen(QHostAddress::Any, tryPort)) {
            port = tryPort;
            emit serverStarted();
            return;
        }
    }

    emit serverError(QString("Could not bind to any port between %1-%2")
                         .arg(startPort)
                         .arg(startPort + 10));
}

void HttpServer::stop()
{
    if (server->isListening()) {
        server->close();
    }
}

int HttpServer::getPort() const { return port; }

void HttpServer::onNewConnection()
{
    QTcpSocket *socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &HttpServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this,
            &HttpServer::onDisconnected);
}

void HttpServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    QByteArray data = socket->readAll();
    QString request = QString::fromUtf8(data);

    // Parse HTTP request
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty())
        return;

    QString requestLine = lines.first();
    QStringList parts = requestLine.split(" ");
    if (parts.size() < 2)
        return;

    QString method = parts[0];
    QString path = parts[1];

    if (method == "GET") {
        handleRequest(socket, path);
    } else {
        sendResponse(socket, 405, "text/plain", "Method Not Allowed");
    }
}

void HttpServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

void HttpServer::handleRequest(QTcpSocket *socket, const QString &path)
{
    // Serve JSON manifest
    if (path == QString("/%1").arg(jsonFileName)) {
        sendJsonManifest(socket);
        return;
    }

    // Serve files from /serve/ directory
    if (path.startsWith("/serve/")) {
        QString encodedFileName = path.mid(7); // Remove "/serve/"
        QString fileName = QUrl::fromPercentEncoding(encodedFileName.toUtf8());

        // Find the file in our list
        QString targetFile;
        for (const QString &file : fileList) {
            QFileInfo info(file);
            if (info.fileName() == fileName) {
                targetFile = file;
                break;
            }
        }

        if (!targetFile.isEmpty()) {
            sendFile(socket, targetFile);
            return;
        }
    }

    sendResponse(socket, 404, "text/html",
                 "<html><body><h1>404 Not Found</h1><p>The requested file was "
                 "not found.</p></body></html>");
}

void HttpServer::sendResponse(QTcpSocket *socket, int statusCode,
                              const QString &contentType,
                              const QByteArray &data)
{
    QString statusText;
    switch (statusCode) {
    case 200:
        statusText = "OK";
        break;
    case 404:
        statusText = "Not Found";
        break;
    case 405:
        statusText = "Method Not Allowed";
        break;
    case 500:
        statusText = "Internal Server Error";
        break;
    default:
        statusText = "Unknown";
        break;
    }

    QString response =
        QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText);
    response += QString("Content-Type: %1\r\n").arg(contentType);
    response += QString("Content-Length: %1\r\n").arg(data.size());
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";

    socket->write(response.toUtf8());
    socket->write(data);
    socket->disconnectFromHost();
}

void HttpServer::sendFile(QTcpSocket *socket, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        sendResponse(socket, 404, "text/plain", "File not found");
        return;
    }

    QByteArray data = file.readAll();
    QString mimeType = getMimeType(filePath);

    // Emit progress signal
    QFileInfo info(filePath);
    emit downloadProgress(info.fileName(), data.size(), data.size());

    sendResponse(socket, 200, mimeType, data);
}

void HttpServer::sendJsonManifest(QTcpSocket *socket)
{
    QString jsonContent = generateJsonManifest();
    sendResponse(socket, 200, "application/json", jsonContent.toUtf8());
}

QString HttpServer::generateJsonManifest() const
{
    QString serverIP = getLocalIP();

    QJsonObject manifest;
    QJsonArray items;

    for (const QString &file : fileList) {
        QFileInfo info(file);
        QJsonObject item;
        item["path"] = QString("http://%1:%2/serve/%3")
                           .arg(serverIP)
                           .arg(port)
                           .arg(QString::fromUtf8(
                               QUrl::toPercentEncoding(info.fileName())));
        items.append(item);
    }

    manifest["items"] = items;

    QJsonDocument doc(manifest);
    return doc.toJson();
}

QString HttpServer::getLocalIP() const
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

QString HttpServer::getMimeType(const QString &filePath) const
{
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(filePath);
    return type.name();
}
