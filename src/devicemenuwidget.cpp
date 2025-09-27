#include "devicemenuwidget.h"
#include "deviceinfowidget.h"
#include "fileexplorerwidget.h"
#include "gallerywidget.h"
#include "iDescriptor.h"
#include "installedappswidget.h"
#include <QDebug>
#include <QTabWidget>
#include <QVBoxLayout>

DeviceMenuWidget::DeviceMenuWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget{parent}, device(device)
{

    QWidget *centralWidget = new QWidget(this);
    tabWidget = new QTabWidget(this);
    tabWidget->tabBar()->hide();
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(tabWidget);

    FileExplorerWidget *explorer = new FileExplorerWidget(device, this);
    explorer->setMinimumHeight(300);

    GalleryWidget *gallery = new GalleryWidget(device, this);
    gallery->setMinimumHeight(300);
    setLayout(mainLayout);

    tabWidget->addTab(new DeviceInfoWidget(device, this), "");
    tabWidget->addTab(new InstalledAppsWidget(device, this), "");
    unsigned int galleryIndex = tabWidget->addTab(gallery, "");
    tabWidget->addTab(explorer, "");

    // TODO : one time ?
    connect(tabWidget, &QTabWidget::currentChanged, this,
            [this, galleryIndex, gallery](int index) {
                if (index == galleryIndex) {
                    qDebug() << "Switched to Gallery tab";
                    gallery->load();
                }
            });
}

void DeviceMenuWidget::switchToTab(const QString &tabName)
{
    if (tabName == "Info") {
        tabWidget->setCurrentIndex(0);
    } else if (tabName == "Apps") {
        tabWidget->setCurrentIndex(1);
    } else if (tabName == "Gallery") {
        tabWidget->setCurrentIndex(2);
    } else if (tabName == "Files") {
        tabWidget->setCurrentIndex(3);
    } else {
        qDebug() << "Tab not found:" << tabName;
    }
}
