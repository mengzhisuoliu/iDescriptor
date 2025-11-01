#ifndef REALTIMESCREEN_H
#define REALTIMESCREEN_H

#include "iDescriptor.h"
#include <QLabel>
#include <QTimer>
#include <QWidget>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/screenshotr.h>

class RealtimeScreenWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RealtimeScreenWidget(iDescriptorDevice *device,
                                  QWidget *parent = nullptr);
    ~RealtimeScreenWidget();

private:
    bool initializeScreenshotService(bool notify);
    void updateScreenshot();
    void startCapturing();

    iDescriptorDevice *m_device;
    QTimer *m_timer;
    QLabel *m_imageLabel;
    QLabel *m_statusLabel;
    screenshotr_client_t m_shotrClient;
    int m_fps;

signals:
};

#endif // REALTIMESCREEN_H
