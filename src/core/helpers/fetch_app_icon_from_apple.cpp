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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPixmap>

void fetchAppIconFromApple(
    QNetworkAccessManager *manager, const QString &bundleId,
    std::function<void(const QPixmap &, const QJsonObject &)> callback)
{
    QString url =
        QString("https://itunes.apple.com/lookup?bundleId=%1").arg(bundleId);

    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(url)));
    QObject::connect(
        reply, &QNetworkReply::finished, [reply, callback, manager]() {
            QByteArray data = reply->readAll();
            reply->deleteLater();

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                callback(QPixmap(), QJsonObject());
                return;
            }

            QJsonObject obj = doc.object();
            QJsonArray results = obj.value("results").toArray();
            if (results.isEmpty()) {
                callback(QPixmap(), QJsonObject());
                return;
            }

            QJsonObject appInfo = results.at(0).toObject();
            QString iconUrl = appInfo.value("artworkUrl100").toString();
            if (iconUrl.isEmpty()) {
                callback(QPixmap(), appInfo);
                return;
            }

            // Fetch the icon image
            QNetworkReply *iconReply =
                manager->get(QNetworkRequest(QUrl(iconUrl)));
            QObject::connect(iconReply, &QNetworkReply::finished,
                             [iconReply, callback, appInfo]() {
                                 QByteArray iconData = iconReply->readAll();
                                 iconReply->deleteLater();
                                 QPixmap pixmap;
                                 pixmap.loadFromData(iconData);
                                 callback(pixmap, appInfo);
                             });
        });
}
