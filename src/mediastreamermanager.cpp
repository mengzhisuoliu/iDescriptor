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

#include "mediastreamermanager.h"
#include <QDebug>
#include <QMutexLocker>

MediaStreamerManager::~MediaStreamerManager() { cleanup(); }

MediaStreamerManager *MediaStreamerManager::sharedInstance()
{
    static MediaStreamerManager instance;
    return &instance;
}

QUrl MediaStreamerManager::getStreamUrl(
    const std::shared_ptr<iDescriptorDevice> device,
    std::optional<std::shared_ptr<CXX::HauseArrest>> hause_arrest, bool useAfc2,
    const QString &filePath)
{
    QString rustUrl;

    if (useAfc2) {
        qDebug() << "Requesting stream URL using Afc2Backend for:" << filePath;
        rustUrl = device->afc2_backend->start_video_stream(filePath);
    } else if (hause_arrest.has_value() && hause_arrest.value()) {
        qDebug() << "Requesting stream URL using HauseArrest for:" << filePath;
        rustUrl = hause_arrest.value()->start_video_stream(filePath);
    } else {
        qDebug() << "Requesting stream URL using AfcBackend for:" << filePath;
        rustUrl = device->afc_backend->start_video_stream(filePath);
    }

    if (rustUrl.isEmpty()) {
        qWarning() << "MediaStreamerManager: start_video_stream failed for"
                   << filePath;
        return {};
    }

    QMutexLocker locker(&m_streamersMutex);
    auto it = m_streamers.find(filePath);
    if (it != m_streamers.end()) {
        it->refCount++;
        qDebug() << "MediaStreamerManager: Reusing existing streamer for"
                 << filePath << "refCount:" << it->refCount;
    } else {
        qDebug() << "MediaStreamerManager: Creating new streamer for"
                 << filePath;
        StreamerInfo info{rustUrl, 1};
        m_streamers.insert(filePath, info);
    }

    return QUrl(rustUrl);
}

void MediaStreamerManager::releaseStreamer(const QString &udid,
                                           const QString &filePath)
{
    QMutexLocker locker(&m_streamersMutex);
    auto it = m_streamers.find(filePath);
    if (it != m_streamers.end()) {
        it->refCount--;
        qDebug() << "MediaStreamerManager: Released streamer for" << filePath
                 << "refCount:" << it->refCount;

        if (it->refCount <= 0) {
            qDebug() << "MediaStreamerManager: Deleting streamer for"
                     << filePath;
            // delete it->streamer;
            AppContext::sharedInstance()->ioManager->release_video_streamer(
                udid, it.value().rustUrl);
            m_streamers.erase(it);
        }
    }
}

void MediaStreamerManager::cleanup()
{
    QMutexLocker locker(&m_streamersMutex);
    auto it = m_streamers.begin();
    while (it != m_streamers.end()) {
        qDebug() << "MediaStreamerManager: Cleaning up streamer for"
                 << it.key();
        // FIXME: what to do here?
        // if (it->streamer) {
        //     delete it->streamer;
        // }
        // release_streamer(it.value().rustUrl);
        it = m_streamers.erase(it);
    }
}