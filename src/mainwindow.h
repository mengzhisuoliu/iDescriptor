#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "devicemenuwidget.h"
#include "iDescriptor.h"
#include "libirecovery.h"
#include <QMainWindow>
#include <libimobiledevice/libimobiledevice.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void deviceAdded(QString udid); // Signal for device connections

private slots:
    void onRecoveryDeviceAdded(
        QObject *device_info); // Slot for recovery device connections
    void onRecoveryDeviceRemoved(
        QObject *device_info); // Slot for recovery device disconnections
    void onDeviceInitFailed(QString udid, lockdownd_error_t err);
    void updateNoDevicesConnected();

private:
    std::map<std::string, QWidget *>
        m_device_menu_widgets; // Map to store devices by UDID
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
