#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "ZDownloader.h"
#include "ZUpdater.h"
#include "customtabwidget.h"
#include "devicemanagerwidget.h"
#include "iDescriptor.h"
#include "libirecovery.h"
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>

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
    static MainWindow *sharedInstance();
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public slots:
    void updateNoDevicesConnected();

private:
    void createMenus();

    Ui::MainWindow *ui;
    CustomTabWidget *m_customTabWidget;
    DeviceManagerWidget *m_deviceManager;
    QStackedWidget *m_mainStackedWidget;
    QLabel *m_connectedDeviceCountLabel;
    ZUpdater *m_updater = nullptr;
};
#endif // MAINWINDOW_H
