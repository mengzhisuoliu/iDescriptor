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

signals:
    void fileSelected(const QString &filePath);

private slots:
    void goBack();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onBreadcrumbClicked();
    void onFileListContextMenu(const QPoint &pos);
    void onExportClicked();
    void onExportDeleteClicked();
    void onImportClicked();
    void onSidebarItemClicked(QTreeWidgetItem *item, int column);
    void onAddToFavoritesClicked();
    void onTryInstallAFC2Clicked();

private:
    QSplitter *mainSplitter;
    QTreeWidget *sidebarTree;
    QWidget *fileExplorerWidget;
    QPushButton *backBtn;
    QPushButton *exportBtn;
    QPushButton *exportDeleteBtn;
    QPushButton *importBtn;
    QPushButton *addToFavoritesBtn;
    QListWidget *fileList;
    QStack<QString> history;
    QHBoxLayout *breadcrumbLayout;
    iDescriptorDevice *device;

    // Current AFC mode
    bool usingAFC2;
    afc_client_t currentAfcClient;

    // Tree items
    QTreeWidgetItem *afcDefaultItem;
    QTreeWidgetItem *afcJailbrokenItem;
    QTreeWidgetItem *commonPlacesItem;
    QTreeWidgetItem *favoritePlacesItem;

    void setupSidebar();
    void setupFileExplorer();
    void loadPath(const QString &path);
    void updateBreadcrumb(const QString &path);
    void loadFavoritePlaces();
    void saveFavoritePlace(const QString &path, const QString &alias);
    void refreshFavoritePlaces();
    void switchToAFC(bool useAFC2);

    void setupContextMenu();
    void exportSelectedFile(QListWidgetItem *item);
    void exportSelectedFile(QListWidgetItem *item, const QString &directory);
    int export_file_to_path(afc_client_t afc, const char *device_path,
                            const char *local_path);
    int import_file_to_device(afc_client_t afc, const char *device_path,
                              const char *local_path);
};

#endif // FILEEXPLORERWIDGET_H
