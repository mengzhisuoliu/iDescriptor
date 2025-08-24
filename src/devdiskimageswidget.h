#ifndef DEVDISKIMAGESWIDGET_H
#define DEVDISKIMAGESWIDGET_H

#include "iDescriptor.h"
#include <QMap>
#include <QWidget>

class QNetworkAccessManager;
class QNetworkReply;
class QListWidget;
class QStackedWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QComboBox;

class DevDiskImagesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DevDiskImagesWidget(iDescriptorDevice *device,
                                 QWidget *parent = nullptr);

private slots:
    void fetchImageList();
    void onImageListFetchFinished(QNetworkReply *reply);
    void onDownloadButtonClicked();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFileDownloadFinished();
    void changeDownloadDirectory();
    void updateDeviceList();
    void onMountButtonClicked();

private:
    void setupUi();
    void parseAndDisplayImages(const QByteArray &jsonData);
    void startDownload(const QString &version);
    void mountImage(const QString &version);
    void onDeviceSelectionChanged(int index);
    void closeEvent(QCloseEvent *event) override;
    struct DownloadItem {
        QNetworkReply *dmgReply = nullptr;
        QNetworkReply *sigReply = nullptr;
        QProgressBar *progressBar = nullptr;
        QPushButton *downloadButton = nullptr;
        QString version;
        qint64 totalSize = 0;
        qint64 totalReceived = 0;
        qint64 dmgReceived = 0;
        qint64 sigReceived = 0;
    };

    QStackedWidget *m_stackedWidget;
    QListWidget *m_imageListWidget;
    QLabel *m_statusLabel;
    QLabel *m_initialStatusLabel;
    QWidget *m_errorWidget;
    QLineEdit *m_downloadPathEdit;
    QComboBox *m_deviceComboBox;
    QPushButton *m_mountButton;

    QNetworkAccessManager *m_networkManager;
    QString m_downloadPath;
    iDescriptorDevice *m_currentDevice;

    QMap<QString, QPair<QString, QString>>
        m_availableImages; // version -> {dmg_path, sig_path}
    QMap<QNetworkReply *, DownloadItem *> m_activeDownloads;
    QByteArray m_imageListJsonData;
};

#endif // DEVDISKIMAGESWIDGET_H
