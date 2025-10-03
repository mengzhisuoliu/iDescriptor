#ifndef AFCEXPLORER_H
#define AFCEXPLORER_H

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

class AfcExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AfcExplorerWidget(
        afc_client_t afcClient = nullptr,
        std::function<void()> onClientInvalidCb = nullptr,
        iDescriptorDevice *device = nullptr, QWidget *parent = nullptr);
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
    QWidget *m_explorer;
    QPushButton *m_backBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_exportDeleteBtn;
    QPushButton *m_importBtn;
    QPushButton *m_addToFavoritesBtn;
    QListWidget *m_fileList;
    QStack<QString> m_history;
    QHBoxLayout *m_breadcrumbLayout;
    iDescriptorDevice *m_device;

    // Current AFC mode
    bool m_usingAFC2;
    afc_client_t m_currentAfcClient;

    void setupFileExplorer();
    void loadPath(const QString &path);
    void updateBreadcrumb(const QString &path);
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

#endif // AFCEXPLORER_H
