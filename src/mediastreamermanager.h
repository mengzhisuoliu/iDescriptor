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

#ifndef MEDIASTREAMERMANAGER_H
#define MEDIASTREAMERMANAGER_H

#include "appcontext.h"
#include "iDescriptor.h"
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QUrl>

class MediaStreamerManager
{
public:
    static MediaStreamerManager *sharedInstance();

    QUrl
    getStreamUrl(const std::shared_ptr<iDescriptorDevice> device,
                 std::optional<std::shared_ptr<CXX::HauseArrest>> hause_arrest,
                 const QString &filePath);

    void releaseStreamer(const QString &filePath);

    void cleanup();

private:
    ~MediaStreamerManager();

private:
    struct StreamerInfo {
        QString rustUrl;
        int refCount;
    };
    static QMutex s_instanceMutex;

    QMap<QString, StreamerInfo> m_streamers;
    QMutex m_streamersMutex;
};

#endif // MEDIASTREAMERMANAGER_H
