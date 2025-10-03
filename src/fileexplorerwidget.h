#ifndef FILEEXPLORERWIDGET_H
#define FILEEXPLORERWIDGET_H

#include "iDescriptor.h"
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QStack>
#include <QString>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <libimobiledevice/afc.h>

class FileExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileExplorerWidget(iDescriptorDevice *device,
                                QWidget *parent = nullptr);

private:
    QSplitter *m_mainSplitter;
    afc_client_t currentAfcClient;
    QTreeWidget *m_sidebarTree;
    iDescriptorDevice *device;
    bool usingAFC2;

    // Tree items
    QTreeWidgetItem *m_afcDefaultItem;
    QTreeWidgetItem *m_afcJailbrokenItem;
    QTreeWidgetItem *m_commonPlacesItem;
    QTreeWidgetItem *m_favoritePlacesItem;

    void setupSidebar();
    void loadFavoritePlaces();
};

#endif // FILEEXPLORERWIDGET_H
