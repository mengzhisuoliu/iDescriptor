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

#ifndef APPSTOREMANAGER_H
#define APPSTOREMANAGER_H

#include <QJsonObject>
#include <QObject>
#include <functional>

class AppStoreManager : public QObject
{
    Q_OBJECT

public:
    static AppStoreManager *sharedInstance();

    // Account management
    QJsonObject getAccountInfo();
    void loginWithCallback(
        const QString &email, const QString &password,
        std::function<void(bool success, const QJsonObject &accountInfo)>
            callback);
    void revokeCredentials();

    // App operations
    void searchApps(
        const QString &searchTerm, int limit,
        std::function<void(bool success, const QString &results)> callback);
    void
    downloadApp(const QString &bundleId, const QString &outputPath,
                const QString &externalVersionId, bool acquireLicense,
                std::function<void(int)> completionCallback,
                std::function<void(long long, long long)> progressCallback);
    void cancelDownload(const QString &bundleId);

signals:
    void loginSuccessful(const QJsonObject &accountInfo);
    void loggedOut(const QJsonObject &accountInfo);

private:
    AppStoreManager(QObject *parent = nullptr);
    ~AppStoreManager() = default;

    AppStoreManager(const AppStoreManager &) = delete;
    AppStoreManager &operator=(const AppStoreManager &) = delete;

    bool m_initialized;
    bool initialize();
};

#endif // APPSTOREMANAGER_H