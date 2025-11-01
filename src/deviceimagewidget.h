#ifndef DEVICEIMAGEWIDGET_H
#define DEVICEIMAGEWIDGET_H

#include "iDescriptor.h"
#include "responsiveqlabel.h"
#include <QTimer>
#include <QWidget>

class DeviceImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceImageWidget(iDescriptorDevice *device,
                               QWidget *parent = nullptr);
    ~DeviceImageWidget();

private slots:
    void updateTime();

private:
    QString m_mockupName;
    void setupDeviceImage();
    QString getDeviceMockupPath() const;
    QString getWallpaperPath() const;
    QString getMockupNameFromDisplayName(const QString &displayName) const;
    int getIosVersionFromDevice() const;
    QPixmap createCompositeImage() const;
    QRect findScreenArea(const QPixmap &mockup) const;

    iDescriptorDevice *m_device;
    ResponsiveQLabel *m_imageLabel;
    QTimer *m_timeUpdateTimer;

    QString m_mockupPath;
    QString m_wallpaperPath;
};

#endif // DEVICEIMAGEWIDGET_H