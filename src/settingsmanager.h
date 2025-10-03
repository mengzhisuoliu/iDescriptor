#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QPair>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QDialog>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    static SettingsManager *sharedInstance();

    // Existing methods
    QString devdiskimgpath() const;

    // Favorite Places API
    void saveFavoritePlace(const QString &path, const QString &alias);
    void removeFavoritePlace(const QString &path);
    QList<QPair<QString, QString>>
    getFavoritePlaces() const; // Returns (path, alias) pairs
    bool isFavoritePlace(const QString &path) const;
    QString getFavoritePlaceAlias(const QString &path) const;
    void clearFavoritePlaces();
    void showSettingsDialog();

signals:
    void favoritePlacesChanged();

private:
    QDialog *m_dialog;
    explicit SettingsManager(QObject *parent = nullptr);
    QSettings *m_settings;

    void cleanupFavoritePlaces();

    static const QString FAVORITE_PREFIX;
};

#endif // SETTINGSMANAGER_H
