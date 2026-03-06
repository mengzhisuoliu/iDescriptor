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

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class HttpServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer();

    void start(const QStringList &files);
    void stop();
    int getPort() const;
    QString getJsonFileName() const { return jsonFileName; }
    static QString getLocalIP();

signals:
    void serverStarted();
    void serverError(const QString &error);
    void downloadProgress(const QString &fileName, int bytesDownloaded,
                          int totalBytes);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer *server;
    QStringList fileList;
    int port;
    QString jsonFileName;
    QMap<QString, int> downloadTracker;

    void handleRequest(QTcpSocket *socket, const QString &path);
    void sendResponse(QTcpSocket *socket, int statusCode,
                      const QString &contentType, const QByteArray &data);
    void sendFile(QTcpSocket *socket, const QString &filePath);
    void sendJsonManifest(QTcpSocket *socket);
    QString generateJsonManifest() const;
    QString getMimeType(const QString &filePath) const;
};

#endif // HTTPSERVER_H
