#ifndef IMAGETASK_H
#define IMAGETASK_H

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QRunnable>
#include <QString>

#include "imageloader.h"

class ImageTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ImageTask(const std::shared_ptr<iDescriptorDevice> device,
              const QString &path, unsigned int row, bool scale = true,
              std::optional<std::shared_ptr<CXX::HauseArrest>> hause_arrest =
                  std::nullopt)
        : m_device(device), m_path(path), m_isThumbnail(scale), m_row(row),
          m_hause_arrest(hause_arrest)
    {
        setAutoDelete(true);
    }

signals:
    void finished(const QString &path, const QPixmap &image, unsigned int row);

protected:
    void run() override
    {
        bool isVideo = iDescriptor::Utils::isVideoFile(m_path);

        if (isVideo) {
            QPixmap thumbnail = ImageLoader::generateVideoThumbnailFFmpeg(
                m_device, m_path, THUMBNAIL_SIZE, m_hause_arrest);

            emit finished(m_path, thumbnail, m_row);
        } else {
            if (m_isThumbnail) {
                QPixmap image = ImageLoader::loadThumbnailFromDevice(
                    m_device, m_path, THUMBNAIL_SIZE, m_hause_arrest);
                emit finished(m_path, image, m_row);
            } else {
                qDebug() << "Loading full image for:" << m_path;
                QPixmap image =
                    ImageLoader::loadImage(m_device, m_path, m_hause_arrest);
                emit finished(m_path, image, m_row);
            }
        }
    }

private:
    const std::shared_ptr<iDescriptorDevice> m_device;
    QString m_path;
    bool m_isThumbnail;
    unsigned int m_row;
    std::optional<std::shared_ptr<CXX::HauseArrest>> m_hause_arrest;
};

#endif // IMAGETASK_H
