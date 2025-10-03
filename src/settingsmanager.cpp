#include "settingsmanager.h"
#include "settingswidget.h"
#include <QDebug>
#include <QSettings>

#define DEFAULT_DEVDISKIMGPATH "./devdiskimages"

SettingsManager *SettingsManager::sharedInstance()
{
    static SettingsManager instance;
    return &instance;
}

void SettingsManager::showSettingsDialog()
{
    if (m_dialog) {
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }

    m_dialog = new SettingsWidget();
    m_dialog->setWindowTitle("Settings - iDescriptor");
    m_dialog->setModal(true);
    m_dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_dialog, &QObject::destroyed, [this]() { m_dialog = nullptr; });

    m_dialog->show();
}

SettingsManager::SettingsManager(QObject *parent) : QObject{parent}
{
    m_settings = new QSettings(this);

    // Clean up any invalid favorite places on startup
    cleanupFavoritePlaces();
}

QString SettingsManager::devdiskimgpath() const
{
    return m_settings->value("devdiskimgpath", DEFAULT_DEVDISKIMGPATH)
        .toString();
}

void SettingsManager::saveFavoritePlace(const QString &path,
                                        const QString &alias)
{
    if (path.isEmpty() || alias.isEmpty()) {
        qDebug() << "Cannot save favorite place: path or alias is empty";
        return;
    }

    // Use a key that encodes the path properly
    QString key =
        "favorite_places/" + QString::fromLatin1(path.toUtf8().toBase64());
    m_settings->setValue(key, QStringList() << path << alias);
    m_settings->sync();

    qDebug() << "Saved favorite place:" << alias << "(" << path << ")";
    emit favoritePlacesChanged();
}

void SettingsManager::removeFavoritePlace(const QString &path)
{
    // Use the same encoding as in saveFavoritePlace
    QString key =
        "favorite_places/" + QString::fromLatin1(path.toUtf8().toBase64());
    if (m_settings->contains(key)) {
        m_settings->remove(key);
        m_settings->sync();
        qDebug() << "Removed favorite place:" << path;
        emit favoritePlacesChanged();
    }
}

QList<QPair<QString, QString>> SettingsManager::getFavoritePlaces() const
{
    QList<QPair<QString, QString>> favorites;

    // Get all keys that start with "favorite_places/"
    QStringList allKeys = m_settings->allKeys();
    QStringList favoriteKeys = allKeys.filter("favorite_places/");

    qDebug() << "Found favorite keys:" << favoriteKeys;

    for (const QString &key : favoriteKeys) {
        QStringList value = m_settings->value(key).toStringList();
        if (value.size() >= 2) {
            QString path = value[0];
            QString alias = value[1];
            if (!path.isEmpty() && !alias.isEmpty()) {
                favorites.append(qMakePair(path, alias));
                qDebug() << "Loaded favorite:" << alias << "->" << path;
            }
        }
    }

    // Sort by alias for consistent ordering
    std::sort(
        favorites.begin(), favorites.end(),
        [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
            return a.second.toLower() < b.second.toLower();
        });

    return favorites;
}

bool SettingsManager::isFavoritePlace(const QString &path) const
{
    QString key =
        "favorite_places/" + QString::fromLatin1(path.toUtf8().toBase64());
    return m_settings->contains(key);
}

QString SettingsManager::getFavoritePlaceAlias(const QString &path) const
{
    QString key =
        "favorite_places/" + QString::fromLatin1(path.toUtf8().toBase64());
    QStringList value = m_settings->value(key).toStringList();
    if (value.size() >= 2) {
        return value[1]; // Return alias
    }
    return QString();
}

void SettingsManager::clearFavoritePlaces()
{
    // Get all keys that start with "favorite_places/" and remove them
    QStringList allKeys = m_settings->allKeys();
    QStringList favoriteKeys = allKeys.filter("favorite_places/");

    for (const QString &key : favoriteKeys) {
        m_settings->remove(key);
    }

    m_settings->sync();

    qDebug() << "Cleared all favorite places";
    emit favoritePlacesChanged();
}

void SettingsManager::cleanupFavoritePlaces()
{
    // Get all keys that start with "favorite_places/" and clean them up
    QStringList allKeys = m_settings->allKeys();
    QStringList favoriteKeys = allKeys.filter("favorite_places/");
    QStringList keysToRemove;

    for (const QString &key : favoriteKeys) {
        QStringList value = m_settings->value(key).toStringList();
        if (value.size() < 2 || value[0].isEmpty() || value[1].isEmpty()) {
            keysToRemove.append(key);
        }
    }

    for (const QString &key : keysToRemove) {
        qDebug() << "Removing invalid favorite place key:" << key;
        m_settings->remove(key);
    }

    if (!keysToRemove.isEmpty()) {
        m_settings->sync();
        emit favoritePlacesChanged();
    }
}