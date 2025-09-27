#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QtConcurrent/QtConcurrent>

void fetchAppIconFromApple(const QString &bundleId,
                           std::function<void(const QPixmap &)> callback,
                           QObject *context)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(context);
    QString url =
        QString("https://itunes.apple.com/lookup?bundleId=%1").arg(bundleId);

    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(url)));
    QObject::connect(
        reply, &QNetworkReply::finished, context,
        [reply, callback, manager, context]() {
            QByteArray data = reply->readAll();
            reply->deleteLater();

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                callback(QPixmap());
                manager->deleteLater();
                return;
            }

            QJsonObject obj = doc.object();
            QJsonArray results = obj.value("results").toArray();
            if (results.isEmpty()) {
                callback(QPixmap());
                manager->deleteLater();
                return;
            }

            QJsonObject appInfo = results.at(0).toObject();
            QString iconUrl = appInfo.value("artworkUrl100").toString();
            if (iconUrl.isEmpty()) {
                callback(QPixmap());
                manager->deleteLater();
                return;
            }

            // Fetch the icon image
            QNetworkReply *iconReply =
                manager->get(QNetworkRequest(QUrl(iconUrl)));
            QObject::connect(iconReply, &QNetworkReply::finished, context,
                             [iconReply, callback, manager]() {
                                 QByteArray iconData = iconReply->readAll();
                                 iconReply->deleteLater();
                                 QPixmap pixmap;
                                 pixmap.loadFromData(iconData);
                                 callback(pixmap);
                                 manager->deleteLater();
                             });
        });
}
