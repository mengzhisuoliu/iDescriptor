#include "settingsmanager.h"
#include "settingswidget.h"
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>

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
    // cleanupFavoritePlaces();
}

QString SettingsManager::devdiskimgpath() const
{
    return downloadPath(); // Use the new downloadPath method
}

// Settings implementation
QString SettingsManager::downloadPath() const
{
    return m_settings
        ->value("downloadPath", SettingsManager::homePath() + "/devdiskimages")
        .toString();
}

QString SettingsManager::homePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
           "/.idescriptor";
}

void SettingsManager::setDownloadPath(const QString &path)
{
    m_settings->setValue("downloadPath", path);
    m_settings->sync();
}

bool SettingsManager::autoCheckUpdates() const
{
    return m_settings->value("autoCheckUpdates", true).toBool();
}

void SettingsManager::setAutoCheckUpdates(bool enabled)
{
    m_settings->setValue("autoCheckUpdates", enabled);
    m_settings->sync();
}

bool SettingsManager::autoRaiseWindow() const
{
    return m_settings->value("autoRaiseWindow", true).toBool();
}

void SettingsManager::setAutoRaiseWindow(bool enabled)
{
    m_settings->setValue("autoRaiseWindow", enabled);
    m_settings->sync();
}

bool SettingsManager::switchToNewDevice() const
{
    return m_settings->value("switchToNewDevice", true).toBool();
}

void SettingsManager::setSwitchToNewDevice(bool enabled)
{
    m_settings->setValue("switchToNewDevice", enabled);
    m_settings->sync();
}

#ifndef __APPLE__
bool SettingsManager::unmountiFuseOnExit() const
{
    return m_settings->value("unmountiFuseOnExit", true).toBool();
}

void SettingsManager::setUnmountiFuseOnExit(bool enabled)
{
    m_settings->setValue("unmountiFuseOnExit", enabled);
    m_settings->sync();
}
#endif

QString SettingsManager::theme() const
{
    return m_settings->value("theme", "System Default").toString();
}

void SettingsManager::setTheme(const QString &theme)
{
    m_settings->setValue("theme", theme);
    m_settings->sync();
}

int SettingsManager::connectionTimeout() const
{
    return m_settings->value("connectionTimeout", 30).toInt();
}

void SettingsManager::setConnectionTimeout(int seconds)
{
    m_settings->setValue("connectionTimeout", seconds);
    m_settings->sync();
}

void SettingsManager::doIfEnabled(Setting setting, std::function<void()> action)
{
    bool shouldExecute = false;

    switch (setting) {
    case Setting::AutoRaiseWindow:
        shouldExecute = autoRaiseWindow();
        break;
    case Setting::SwitchToNewDevice:
        shouldExecute = switchToNewDevice();
        break;
    case Setting::AutoCheckUpdates:
        shouldExecute = autoCheckUpdates();
        break;
#ifndef __APPLE__
    case Setting::UnmountiFuseOnExit:
        shouldExecute = unmountiFuseOnExit();
        break;
#endif
    default:
        qWarning() << "Unhandled setting in doIfEnabled";
        return;
    }

    qDebug() << "enabled" << switchToNewDevice();
    if (shouldExecute && action) {
        action();
    }
}

void SettingsManager::resetToDefaults()
{
    setDownloadPath(SettingsManager::homePath() + "/devdiskimages");
    setAutoCheckUpdates(true);
    setAutoRaiseWindow(true);
    setSwitchToNewDevice(true);
#ifndef __APPLE__
    setUnmountiFuseOnExit(false);
#endif
    setTheme("System Default");
    setConnectionTimeout(30);
}

void SettingsManager::saveFavoritePlace(const QString &path,
                                        const QString &alias,
                                        const QString &keyPrefix)
{
    if (path.isEmpty() || alias.isEmpty()) {
        qWarning() << "Cannot save favorite place with empty path or alias";
        return;
    }

    // Use a key that encodes the path properly
    QString key = keyPrefix + QString::fromLatin1(path.toUtf8().toBase64());
    m_settings->setValue(key, QStringList() << path << alias);
    m_settings->sync();

    qDebug() << "Saved favorite place (AFC2):" << alias << "(" << path << ")";
    emit favoritePlacesChanged();
}

void SettingsManager::removeFavoritePlace(const QString &keyPrefix,
                                          const QString &path)
{
    // Use the same encoding as in saveFavoritePlace
    QString key = keyPrefix + QString::fromLatin1(path.toUtf8().toBase64());
    qDebug() << "Attempting to remove favorite place with key:" << key;
    if (m_settings->contains(key)) {
        m_settings->remove(key);
        m_settings->sync();
        qDebug() << "Removed favorite place:" << path;
        emit favoritePlacesChanged();
    }
}

QList<QPair<QString, QString>>
SettingsManager::getFavoritePlaces(const QString &keyPrefix) const
{
    QList<QPair<QString, QString>> favorites;

    // Get all keys that start with the specified prefix
    QStringList allKeys = m_settings->allKeys();
    QStringList favoriteKeys = allKeys.filter(keyPrefix);

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

void SettingsManager::clearKeys(const QString &keyPrefix)
{
    QStringList allKeys = m_settings->allKeys();
    QStringList favoriteKeys = allKeys.filter(keyPrefix);

    for (const QString &key : favoriteKeys) {
        m_settings->remove(key);
    }

    m_settings->sync();

    emit favoritePlacesChanged();
}