#include "fileexplorerwidget.h"
#include "afcexplorerwidget.h"
#include "iDescriptor.h"
#include "mediapreviewdialog.h"
#include "settingsmanager.h"
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QVariant>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>

FileExplorerWidget::FileExplorerWidget(iDescriptorDevice *device,
                                       QWidget *parent)
    : QWidget(parent), device(device), usingAFC2(false)
{

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    // Main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_mainSplitter);

    setupSidebar();

    // Add widgets to splitter
    m_mainSplitter->addWidget(m_sidebarTree);
    m_mainSplitter->addWidget(
        new AfcExplorerWidget(device->afcClient, nullptr, device));
    m_mainSplitter->setSizes({400, 800});
    setLayout(mainLayout);
}
// useAFC2 ,path,
typedef QPair<bool, QString> SidebarItemData;

void FileExplorerWidget::setupSidebar()
{
    m_sidebarTree = new QTreeWidget();
    m_sidebarTree->setHeaderLabel("Files");
    m_sidebarTree->setMinimumWidth(350);
    m_sidebarTree->setMaximumWidth(400);

    // AFC Default section
    m_afcDefaultItem = new QTreeWidgetItem(m_sidebarTree);
    m_afcDefaultItem->setText(0, "Explorer");
    m_afcDefaultItem->setIcon(0, QIcon::fromTheme("folder"));
    m_afcDefaultItem->setData(0, Qt::UserRole,
                              QVariant::fromValue(SidebarItemData(false, "/")));
    m_afcDefaultItem->setExpanded(true);

    // Add root folder under Default
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(m_afcDefaultItem);
    rootItem->setText(0, "Default");
    rootItem->setIcon(0, QIcon::fromTheme("folder"));
    rootItem->setData(0, Qt::UserRole,
                      QVariant::fromValue(SidebarItemData(false, "/")));
    rootItem->setData(0, Qt::UserRole + 1, QVariant::fromValue(false));

    // AFC2 Jailbroken section
    m_afcJailbrokenItem = new QTreeWidgetItem(m_afcDefaultItem);
    m_afcJailbrokenItem->setText(0, "Jailbroken (AFC2)");
    m_afcJailbrokenItem->setIcon(0, QIcon::fromTheme("applications-system"));
    m_afcJailbrokenItem->setData(
        0, Qt::UserRole, QVariant::fromValue(SidebarItemData(true, "/")));
    m_afcJailbrokenItem->setExpanded(false);

    // Common Places section
    m_commonPlacesItem = new QTreeWidgetItem(m_sidebarTree);
    m_commonPlacesItem->setText(0, "Common Places");
    m_commonPlacesItem->setIcon(0, QIcon::fromTheme("places-bookmarks"));
    m_commonPlacesItem->setData(
        0, Qt::UserRole,
        QVariant::fromValue(
            SidebarItemData(false, "../../../var/mobile/Library/Wallpapers")));
    m_commonPlacesItem->setExpanded(true);

    QTreeWidgetItem *wallpapersItem = new QTreeWidgetItem(m_commonPlacesItem);
    wallpapersItem->setText(0, "Wallpapers");
    wallpapersItem->setIcon(0, QIcon::fromTheme("image-x-generic"));
    wallpapersItem->setData(
        0, Qt::UserRole,
        QVariant::fromValue(
            SidebarItemData(false, "../../../var/mobile/Library/Wallpapers")));
    wallpapersItem->setData(0, Qt::UserRole + 1,
                            QVariant::fromValue(false)); // Default AFC

    // Favorite Places section
    m_favoritePlacesItem = new QTreeWidgetItem(m_sidebarTree);
    m_favoritePlacesItem->setText(0, "Favorite Places");
    m_favoritePlacesItem->setIcon(0, QIcon::fromTheme("user-bookmarks"));
    m_favoritePlacesItem->setData(
        // todo:implement
        0, Qt::UserRole, QVariant::fromValue(SidebarItemData(false, "/")));
    m_favoritePlacesItem->setExpanded(true);

    loadFavoritePlaces();

    // connect(m_sidebarTree, &QTreeWidget::itemClicked, this,
    //         &FileExplorerWidget::onSidebarItemClicked);
}

void FileExplorerWidget::loadFavoritePlaces()
{
    SettingsManager *settings = SettingsManager::sharedInstance();
    QList<QPair<QString, QString>> favorites = settings->getFavoritePlaces();
    qDebug() << "Loading favorite places:" << favorites.size();
    for (const auto &favorite : favorites) {
        QString path = favorite.first;
        QString alias = favorite.second;

        qDebug() << "Favorite:" << alias << "->" << path;
        QTreeWidgetItem *favoriteItem =
            new QTreeWidgetItem(m_favoritePlacesItem);
        favoriteItem->setText(0, alias);
        favoriteItem->setIcon(0, QIcon::fromTheme("folder-favorites"));
        favoriteItem->setData(
            0, Qt::UserRole, QVariant::fromValue(SidebarItemData(false, path)));
        favoriteItem->setData(0, Qt::UserRole + 1,
                              QVariant::fromValue(false)); // Default to AFC
    }
}