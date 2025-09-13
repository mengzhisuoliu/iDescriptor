#ifndef PHOTOMODEL_H
#define PHOTOMODEL_H

#include "iDescriptor.h"
#include <QAbstractListModel>
#include <QCache>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFutureWatcher>
#include <QPixmap>
#include <QSize>
#include <QStandardPaths>

struct PhotoInfo {
    QString filePath;
    QString fileName;
    QDateTime dateTime;
    bool thumbnailRequested = false;

    enum FileType { Image, Video };
    FileType fileType;
};

class PhotoModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum SortOrder { NewestFirst, OldestFirst };

    enum FilterType { All, ImagesOnly, VideosOnly };

    explicit PhotoModel(iDescriptorDevice *device, QObject *parent = nullptr);
    ~PhotoModel();

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    // Thumbnail management
    void setThumbnailSize(const QSize &size);
    void clearCache();

    // Sorting and filtering
    void setSortOrder(SortOrder order);
    SortOrder sortOrder() const { return m_sortOrder; }

    void setFilterType(FilterType filter);
    FilterType filterType() const { return m_filterType; }

    // Export functionality
    QStringList getSelectedFilePaths(const QModelIndexList &indexes) const;
    QString getFilePath(const QModelIndex &index) const;
    PhotoInfo::FileType getFileType(const QModelIndex &index) const;

    // Get all items for export
    QStringList getAllFilePaths() const;
    QStringList getFilteredFilePaths() const;

signals:
    void thumbnailNeedsLoading(int index);
    void exportRequested(const QStringList &filePaths);

private slots:
    void requestThumbnail(int index);

private:
    // Data members
    iDescriptorDevice *m_device;
    QList<PhotoInfo> m_allPhotos; // All photos from device
    QList<PhotoInfo> m_photos;    // Currently filtered/sorted photos

    // Thumbnail management
    QSize m_thumbnailSize;
    mutable QCache<QString, QPixmap> m_thumbnailCache;
    QString m_cacheDir;
    mutable QHash<QString, QFutureWatcher<QPixmap> *> m_activeLoaders;
    mutable QSet<QString> m_loadingPaths;

    // Sorting and filtering
    SortOrder m_sortOrder;
    FilterType m_filterType;

    // Helper methods
    void populatePhotoPaths();
    void applyFilterAndSort();
    void sortPhotos(QList<PhotoInfo> &photos) const;
    bool matchesFilter(const PhotoInfo &info) const;

    QString getThumbnailCacheKey(const QString &filePath) const;
    QString getThumbnailCachePath(const QString &filePath) const;

    QDateTime extractDateTimeFromFile(const QString &filePath) const;
    PhotoInfo::FileType determineFileType(const QString &fileName) const;

    // Static helper methods
    static QPixmap loadThumbnailFromDevice(iDescriptorDevice *device,
                                           const QString &filePath,
                                           const QSize &size,
                                           const QString &cachePath);

    static QPixmap generateVideoThumbnail(iDescriptorDevice *device,
                                          const QString &filePath,
                                          const QSize &requestedSize);
};

#endif // PHOTOMODEL_H