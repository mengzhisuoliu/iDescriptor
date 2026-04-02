#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include "iDescriptor.h"
#include <QCache>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

class ImageTask;

typedef struct AfcClient *AfcClientHandle;

class ImageLoader : public QObject
{
    Q_OBJECT
public:
    explicit ImageLoader(QObject *parent = nullptr);
    static ImageLoader &sharedInstance()
    {
        static ImageLoader instance;
        return instance;
    }
    void requestThumbnail(const std::shared_ptr<iDescriptorDevice> device,
                          const QString &path, unsigned int row = 0);
    void requestImageWithCallback(
        const std::shared_ptr<iDescriptorDevice> device, const QString &path,
        int priority, std::function<void(const QPixmap &)> callback,
        std::optional<std::shared_ptr<CXX::HauseArrest>> hause_arrest =
            std::nullopt);
    void cancelThumbnail(const QString &path);
    bool isLoading(const QString &path);
    void clear();
    QCache<QString, QPixmap> m_cache;
    static QPixmap loadThumbnailFromDevice(
        const std::shared_ptr<iDescriptorDevice> device,
        const QString &filePath, const QSize &size,
        std::optional<std::shared_ptr<CXX::HauseArrest>> hause_arrest =
            std::nullopt);
    static QPixmap generateVideoThumbnailFFmpeg(
        const std::shared_ptr<iDescriptorDevice> device,
        const QString &filePath, const QSize &size,
        std::optional<std::shared_ptr<CXX::HauseArrest>> hause_arrest =
            std::nullopt);
    static QPixmap loadImage(const std::shared_ptr<iDescriptorDevice> device,
                             const QString &filePath,
                             std::optional<std::shared_ptr<CXX::HauseArrest>>
                                 hause_arrest = std::nullopt);
signals:
    void thumbnailReady(const QString &path, const QPixmap &image,
                        unsigned int row);

private slots:
    void onTaskFinished(const QString &path, const QPixmap &image,
                        unsigned int row);

private:
    QThreadPool m_pool;
    QHash<QString, ImageTask *> m_pendingTasks;
    QMutex m_mutex;
};

#endif // IMAGELOADER_H