#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QDialog>
#include <QObject>
#include <QPair>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <functional>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    static SettingsManager *sharedInstance();

    // Settings keys
    enum class Setting {
        DownloadPath,
        AutoCheckUpdates,
        AutoRaiseWindow,
        SwitchToNewDevice,
        UnmountiFuseOnExit,
        Theme,
        ConnectionTimeout
    };
    static QString homePath();
    // Existing methods
    QString devdiskimgpath() const;
    void clearKeys(const QString &keyPrefix);

    void saveFavoritePlace(const QString &path, const QString &alias,
                           const QString &keyPrefix);
    void removeFavoritePlace(const QString &keyPrefix, const QString &path);
    QList<QPair<QString, QString>>
    getFavoritePlaces(const QString &keyPrefix) const;
    void showSettingsDialog();

    // New settings methods
    QString downloadPath() const;
    void setDownloadPath(const QString &path);

    bool autoCheckUpdates() const;
    void setAutoCheckUpdates(bool enabled);

    bool autoRaiseWindow() const;
    void setAutoRaiseWindow(bool enabled);

    bool switchToNewDevice() const;
    void setSwitchToNewDevice(bool enabled);

#ifndef __APPLE__
    bool unmountiFuseOnExit() const;
    void setUnmountiFuseOnExit(bool enabled);
#endif

    QString theme() const;
    void setTheme(const QString &theme);

    int connectionTimeout() const;
    void setConnectionTimeout(int seconds);

    // Utility method for conditional execution
    void doIfEnabled(Setting setting, std::function<void()> action);

    // Reset to defaults
    void resetToDefaults();

signals:
    void favoritePlacesChanged();

private:
    QDialog *m_dialog;
    explicit SettingsManager(QObject *parent = nullptr);
    QSettings *m_settings;

    static const QString FAVORITE_PREFIX;
};

#endif // SETTINGSMANAGER_H
