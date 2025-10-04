#include "devicemenuwidget.h"
#include "deviceinfowidget.h"
#include "fileexplorerwidget.h"
#include "gallerywidget.h"
#include "iDescriptor.h"
#include "installedappswidget.h"
#include <QDebug>
#include <QStackedWidget>
#include <QVBoxLayout>

DeviceMenuWidget::DeviceMenuWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget{parent}, device(device)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setContentsMargins(0, 0, 0, 0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(stackedWidget);

    // Create and add widgets to the stacked widget
    DeviceInfoWidget *deviceInfoWidget = new DeviceInfoWidget(device, this);
    InstalledAppsWidget *installedAppsWidget =
        new InstalledAppsWidget(device, this);
    GalleryWidget *galleryWidget = new GalleryWidget(device, this);
    FileExplorerWidget *fileExplorerWidget =
        new FileExplorerWidget(device, this);

    // Set minimum heights
    galleryWidget->setMinimumHeight(300);
    fileExplorerWidget->setMinimumHeight(300);

    // Add widgets to stack (index 0, 1, 2, 3)
    stackedWidget->addWidget(deviceInfoWidget);    // Index 0 - Info
    stackedWidget->addWidget(installedAppsWidget); // Index 1 - Apps
    stackedWidget->addWidget(galleryWidget);       // Index 2 - Gallery
    stackedWidget->addWidget(fileExplorerWidget);  // Index 3 - Files

    // Set default to Info tab
    stackedWidget->setCurrentIndex(0);

    // Connect to current changed signal for lazy loading
    connect(stackedWidget, &QStackedWidget::currentChanged, this,
            [this, galleryWidget](int index) {
                if (index == 2) { // Gallery tab
                    qDebug() << "Switched to Gallery tab";
                    galleryWidget->load();
                }
            });
}

void DeviceMenuWidget::switchToTab(const QString &tabName)
{
    if (tabName == "Info") {
        stackedWidget->setCurrentIndex(0);
    } else if (tabName == "Apps") {
        stackedWidget->setCurrentIndex(1);
    } else if (tabName == "Gallery") {
        stackedWidget->setCurrentIndex(2);
    } else if (tabName == "Files") {
        stackedWidget->setCurrentIndex(3);
    } else {
        qDebug() << "Tab not found:" << tabName;
    }
}
