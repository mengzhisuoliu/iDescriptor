#ifndef DEVDISKIMAGESWIDGET_H
#define DEVDISKIMAGESWIDGET_H

#include "iDescriptor.h"
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QNetworkReply>
#include <QPair>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QWidget>

class DevDiskImagesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DevDiskImagesWidget(iDescriptorDevice *device,
                                 QWidget *parent = nullptr);

private slots:
    void fetchImages();
    void onDownloadButtonClicked();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFileDownloadFinished();
    void updateDeviceList();
    void onMountButtonClicked();
    void onImageListFetched(bool success,
                            const QString &errorMessage = QString());

private:
    void setupUi();
    void displayImages();
    void startDownload(const QString &version);
    void mountImage(const QString &version);
    void onDeviceSelectionChanged(int index);
    void closeEvent(QCloseEvent *event) override;
    void checkMountedImage();

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

    char *m_mounted_sig = NULL;
    uint64_t m_mounted_sig_len = 0;

    QStackedWidget *m_stackedWidget;
    QListWidget *m_imageListWidget;
    QLabel *m_statusLabel;
    QLabel *m_initialStatusLabel;
    QWidget *m_errorWidget;
    QComboBox *m_deviceComboBox;
    QPushButton *m_mountButton;
    QPushButton *m_check_mountedButton;

    iDescriptorDevice *m_currentDevice;
    QStringList m_compatibleVersions;
    QStringList m_otherVersions;

    QMap<QString, QPair<QString, QString>>
        m_availableImages; // version -> {dmg_path, sig_path}
    QMap<QNetworkReply *, DownloadItem *> m_activeDownloads;
};

#endif // DEVDISKIMAGESWIDGET_H
