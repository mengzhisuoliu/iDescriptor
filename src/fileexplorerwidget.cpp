#include "fileexplorerwidget.h"
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
    // Initialize current AFC client to default
    currentAfcClient = device->afcClient;
    qDebug() << "AFC2 available:" << (device->afc2Client != nullptr);
    // Create main splitter
    mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Setup sidebar
    setupSidebar();

    // Setup file explorer
    setupFileExplorer();

    // Add widgets to splitter
    mainSplitter->addWidget(sidebarTree);
    mainSplitter->addWidget(fileExplorerWidget);
    mainSplitter->setSizes({400, 800});

    // Main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(mainSplitter);
    setLayout(mainLayout);

    // Initialize
    history.push("/");
    loadPath("/");

    setupContextMenu();
    connect(SettingsManager::sharedInstance(),
            &SettingsManager::favoritePlacesChanged, this,
            &FileExplorerWidget::refreshFavoritePlaces);
}

void FileExplorerWidget::goBack()
{
    if (history.size() > 1) {
        history.pop();
        QString prevPath = history.top();
        loadPath(prevPath);
    }
}

void FileExplorerWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    QVariant data = item->data(Qt::UserRole);
    bool isDir = data.toBool();
    QString name = item->text();

    // Use breadcrumb to get current path
    QString currPath = "/";
    if (!history.isEmpty())
        currPath = history.top();

    if (!currPath.endsWith("/"))
        currPath += "/";
    QString nextPath = currPath == "/" ? "/" + name : currPath + name;

    if (isDir) {
        history.push(nextPath);
        loadPath(nextPath);
    } else {
        auto *previewDialog = new MediaPreviewDialog(device, nextPath, this);
        previewDialog->setAttribute(Qt::WA_DeleteOnClose);
        previewDialog->show();
        // TODO: we need this ?
        emit fileSelected(nextPath);
    }
}

void FileExplorerWidget::onBreadcrumbClicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    QString path = btn->property("fullPath").toString();
    // pathLabel removed, compare with history.top()
    if (!history.isEmpty() && path == history.top())
        return;
    history.push(path);
    loadPath(path);
}

void FileExplorerWidget::updateBreadcrumb(const QString &path)
{
    // Remove old breadcrumb buttons
    QLayoutItem *child;
    while ((child = breadcrumbLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    QStringList parts = path.split("/", Qt::SkipEmptyParts);
    QString currPath = "";
    int idx = 0;
    // Add root
    QPushButton *rootBtn = new QPushButton("/");
    rootBtn->setFlat(true);
    rootBtn->setProperty("fullPath", "/");
    connect(rootBtn, &QPushButton::clicked, this,
            &FileExplorerWidget::onBreadcrumbClicked);
    breadcrumbLayout->addWidget(rootBtn);

    for (const QString &part : parts) {
        currPath += part;
        if (idx > 0) {
            QLabel *sep = new QLabel(" / ");
            breadcrumbLayout->addWidget(sep);
        }

        QPushButton *btn = new QPushButton(part);
        btn->setFlat(true);
        btn->setProperty("fullPath", currPath);
        connect(btn, &QPushButton::clicked, this,
                &FileExplorerWidget::onBreadcrumbClicked);
        breadcrumbLayout->addWidget(btn);
        idx++;
    }
    breadcrumbLayout->addStretch();
}

void FileExplorerWidget::loadPath(const QString &path)
{
    fileList->clear();
    // pathLabel->setText(path); // removed

    updateBreadcrumb(path);

    MediaFileTree tree =
        get_file_tree(currentAfcClient, device->device, path.toStdString());
    if (!tree.success) {
        fileList->addItem("Failed to load directory");
        return;
    }

    for (const auto &entry : tree.entries) {
        QListWidgetItem *item =
            new QListWidgetItem(QString::fromStdString(entry.name));
        item->setData(Qt::UserRole, entry.isDir);
        if (entry.isDir)
            item->setIcon(QIcon::fromTheme("folder"));
        else
            item->setIcon(QIcon::fromTheme("text-x-generic"));
        fileList->addItem(item);
    }
}

void FileExplorerWidget::setupContextMenu()
{
    fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fileList, &QListWidget::customContextMenuRequested, this,
            &FileExplorerWidget::onFileListContextMenu);
}

void FileExplorerWidget::onFileListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = fileList->itemAt(pos);
    if (!item)
        return;

    bool isDir = item->data(Qt::UserRole).toBool();
    if (isDir)
        return; // Only export files

    QMenu menu;
    QAction *exportAction = menu.addAction("Export");
    QAction *selectedAction = menu.exec(fileList->viewport()->mapToGlobal(pos));
    if (selectedAction == exportAction) {
        QList<QListWidgetItem *> selectedItems = fileList->selectedItems();
        QList<QListWidgetItem *> filesToExport;
        if (selectedItems.isEmpty())
            filesToExport.append(item); // fallback: just the clicked one
        else {
            for (QListWidgetItem *selItem : selectedItems) {
                if (!selItem->data(Qt::UserRole).toBool())
                    filesToExport.append(selItem);
            }
        }
        if (filesToExport.isEmpty())
            return;
        QString dir =
            QFileDialog::getExistingDirectory(this, "Select Export Directory");
        if (dir.isEmpty())
            return;
        for (QListWidgetItem *selItem : filesToExport) {
            exportSelectedFile(selItem, dir);
        }
    }
}

void FileExplorerWidget::onExportClicked()
{
    QList<QListWidgetItem *> selectedItems = fileList->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // Only files (not directories)
    QList<QListWidgetItem *> filesToExport;
    for (QListWidgetItem *item : selectedItems) {
        if (!item->data(Qt::UserRole).toBool())
            filesToExport.append(item);
    }
    if (filesToExport.isEmpty())
        return;

    // Ask user for a directory to save all files
    QString dir =
        QFileDialog::getExistingDirectory(this, "Select Export Directory");
    if (dir.isEmpty())
        return;

    for (QListWidgetItem *item : filesToExport) {
        exportSelectedFile(item, dir);
    }
}

void FileExplorerWidget::onExportDeleteClicked()
{
    // Placeholder for future implementation
    QList<QListWidgetItem *> selectedItems = fileList->selectedItems();
    if (selectedItems.isEmpty())
        return;
    // TODO: Implement export & delete logic
    return;
}

void FileExplorerWidget::exportSelectedFile(QListWidgetItem *item,
                                            const QString &directory)
{
    QString fileName = item->text();
    QString currPath = "/";
    if (!history.isEmpty())
        currPath = history.top();
    if (!currPath.endsWith("/"))
        currPath += "/";
    qDebug() << "Current path:" << currPath;
    QString devicePath = currPath == "/" ? "/" + fileName : currPath + fileName;
    qDebug() << "Exporting file:" << devicePath;

    // Save to selected directory
    QString savePath = directory + "/" + fileName;

    // Get device info and check connections
    qDebug() << "Using device index:" << device->udid.c_str();
    qDebug() << "Device UDID:" << QString::fromStdString(device->udid);
    qDebug() << "Device Product Type:"
             << QString::fromStdString(device->deviceInfo.productType);

    // Export file using the validated connections
    int result =
        export_file_to_path(currentAfcClient, devicePath.toStdString().c_str(),
                            savePath.toStdString().c_str());

    qDebug() << "Export result:" << result;

    if (result == 0) {
        qDebug() << "Exported" << devicePath << "to" << savePath;

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(
            this, "Export Successful",
            "File exported successfully. Would you like to see the directory?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
        }
    } else {
        qDebug() << "Failed to export" << devicePath;
        QMessageBox::warning(this, "Export Failed",
                             "Failed to export the file from the device");
    }
}

// Helper function to export to a specific local path
int FileExplorerWidget::export_file_to_path(afc_client_t afc,
                                            const char *device_path,
                                            const char *local_path)
{
    uint64_t handle = 0;
    // TODO: implement safe_afc_file_open
    if (afc_file_open(afc, device_path, AFC_FOPEN_RDONLY, &handle) !=
        AFC_E_SUCCESS) {
        qDebug() << "Failed to open file on device:" << device_path;
        return -1;
    }
    FILE *out = fopen(local_path, "wb");
    if (!out) {
        qDebug() << "Failed to open local file:" << local_path;
        afc_file_close(afc, handle);
        return -1;
    }

    char buffer[4096];
    uint32_t bytes_read = 0;
    // TODO: implement safe_afc_file_read
    while (afc_file_read(afc, handle, buffer, sizeof(buffer), &bytes_read) ==
               AFC_E_SUCCESS &&
           bytes_read > 0) {
        fwrite(buffer, 1, bytes_read, out);
    }

    fclose(out);
    // TODO: implement safe_afc_file_close
    afc_file_close(afc, handle);
    return 0;
}

void FileExplorerWidget::onImportClicked()
{
    // TODO: check devices

    // Select one or more files to import
    QStringList fileNames = QFileDialog::getOpenFileNames(this, "Import Files");
    if (fileNames.isEmpty())
        return;

    // Use current breadcrumb directory as target
    QString currPath = "/";
    if (!history.isEmpty())
        currPath = history.top();
    if (!currPath.endsWith("/"))
        currPath += "/";

    // if (!device || !client || !serviceDesc)
    // {
    //     qDebug() << "Failed to connect to device or lockdown service";
    //     return;
    // }

    // Import each file
    for (const QString &localPath : fileNames) {
        QFileInfo fi(localPath);
        QString devicePath = currPath + fi.fileName();
        int result = import_file_to_device(currentAfcClient,
                                           devicePath.toStdString().c_str(),
                                           localPath.toStdString().c_str());
        if (result == 0)
            qDebug() << "Imported" << localPath << "to" << devicePath;
        else
            qDebug() << "Failed to import" << localPath;
    }

    // Refresh file list
    loadPath(currPath);
}

// Helper function to import a file from a local path to the device
int FileExplorerWidget::import_file_to_device(afc_client_t afc,
                                              const char *device_path,
                                              const char *local_path)
{
    QFile in(local_path);
    if (!in.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open local file for import:" << local_path;
        return -1;
    }

    uint64_t handle = 0;
    if (afc_file_open(afc, device_path, AFC_FOPEN_WRONLY, &handle) !=
        AFC_E_SUCCESS) {
        qDebug() << "Failed to open file on device for writing:" << device_path;
        return -1;
    }

    char buffer[4096];
    qint64 bytesRead;
    while ((bytesRead = in.read(buffer, sizeof(buffer))) > 0) {
        uint32_t bytesWritten = 0;
        if (afc_file_write(afc, handle, buffer,
                           static_cast<uint32_t>(bytesRead),
                           &bytesWritten) != AFC_E_SUCCESS ||
            bytesWritten != bytesRead) {
            qDebug() << "Failed to write to device file:" << device_path;
            afc_file_close(afc, handle);
            in.close();
            return -1;
        }
    }

    afc_file_close(afc, handle);
    in.close();
    return 0;
}

// useAFC2 ,path,
typedef QPair<bool, QString> SidebarItemData;

void FileExplorerWidget::setupSidebar()
{
    sidebarTree = new QTreeWidget();
    sidebarTree->setHeaderLabel("Files");
    sidebarTree->setMinimumWidth(350);
    sidebarTree->setMaximumWidth(400);

    // AFC Default section
    afcDefaultItem = new QTreeWidgetItem(sidebarTree);
    afcDefaultItem->setText(0, "Explorer");
    afcDefaultItem->setIcon(0, QIcon::fromTheme("folder"));
    afcDefaultItem->setData(0, Qt::UserRole,
                            QVariant::fromValue(SidebarItemData(false, "/")));
    afcDefaultItem->setExpanded(true);

    // Add root folder under Default
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(afcDefaultItem);
    rootItem->setText(0, "Default");
    rootItem->setIcon(0, QIcon::fromTheme("folder"));
    rootItem->setData(0, Qt::UserRole,
                      QVariant::fromValue(SidebarItemData(false, "/")));
    rootItem->setData(0, Qt::UserRole + 1, QVariant::fromValue(false));

    // AFC2 Jailbroken section
    afcJailbrokenItem = new QTreeWidgetItem(afcDefaultItem);
    afcJailbrokenItem->setText(0, "Jailbroken (AFC2)");
    afcJailbrokenItem->setIcon(0, QIcon::fromTheme("applications-system"));
    afcJailbrokenItem->setData(0, Qt::UserRole,
                               QVariant::fromValue(SidebarItemData(true, "/")));
    afcJailbrokenItem->setExpanded(false);

    // Common Places section
    commonPlacesItem = new QTreeWidgetItem(sidebarTree);
    commonPlacesItem->setText(0, "Common Places");
    commonPlacesItem->setIcon(0, QIcon::fromTheme("places-bookmarks"));
    commonPlacesItem->setData(
        0, Qt::UserRole,
        QVariant::fromValue(
            SidebarItemData(false, "../../../var/mobile/Library/Wallpapers")));
    commonPlacesItem->setExpanded(true);

    QTreeWidgetItem *wallpapersItem = new QTreeWidgetItem(commonPlacesItem);
    wallpapersItem->setText(0, "Wallpapers");
    wallpapersItem->setIcon(0, QIcon::fromTheme("image-x-generic"));
    wallpapersItem->setData(
        0, Qt::UserRole,
        QVariant::fromValue(
            SidebarItemData(false, "../../../var/mobile/Library/Wallpapers")));
    wallpapersItem->setData(0, Qt::UserRole + 1,
                            QVariant::fromValue(false)); // Default AFC

    // Favorite Places section
    favoritePlacesItem = new QTreeWidgetItem(sidebarTree);
    favoritePlacesItem->setText(0, "Favorite Places");
    favoritePlacesItem->setIcon(0, QIcon::fromTheme("user-bookmarks"));
    favoritePlacesItem->setData(
        // todo:implement
        0, Qt::UserRole, QVariant::fromValue(SidebarItemData(false, "/")));
    favoritePlacesItem->setExpanded(true);

    loadFavoritePlaces();

    connect(sidebarTree, &QTreeWidget::itemClicked, this,
            &FileExplorerWidget::onSidebarItemClicked);
}

void FileExplorerWidget::setupFileExplorer()
{
    fileExplorerWidget = new QWidget();
    QVBoxLayout *explorerLayout = new QVBoxLayout(fileExplorerWidget);

    // Export/Import buttons layout
    QHBoxLayout *exportLayout = new QHBoxLayout();
    exportBtn = new QPushButton("Export");
    exportDeleteBtn = new QPushButton("Export & Delete");
    importBtn = new QPushButton("Import");
    addToFavoritesBtn = new QPushButton("Add to Favorites");
    exportLayout->addWidget(exportBtn);
    exportLayout->addWidget(exportDeleteBtn);
    exportLayout->addWidget(importBtn);
    exportLayout->addWidget(addToFavoritesBtn);
    exportLayout->addStretch();
    explorerLayout->addLayout(exportLayout);

    // Navigation layout (Back + Breadcrumb)
    QHBoxLayout *navLayout = new QHBoxLayout();
    backBtn = new QPushButton("Back");
    breadcrumbLayout = new QHBoxLayout();
    breadcrumbLayout->setSpacing(0);
    navLayout->addWidget(backBtn);
    navLayout->addLayout(breadcrumbLayout);
    navLayout->addStretch();
    explorerLayout->addLayout(navLayout);

    // File list
    fileList = new QListWidget();
    fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    explorerLayout->addWidget(fileList);

    // Connect buttons
    connect(backBtn, &QPushButton::clicked, this, &FileExplorerWidget::goBack);
    connect(fileList, &QListWidget::itemDoubleClicked, this,
            &FileExplorerWidget::onItemDoubleClicked);
    connect(exportBtn, &QPushButton::clicked, this,
            &FileExplorerWidget::onExportClicked);
    connect(exportDeleteBtn, &QPushButton::clicked, this,
            &FileExplorerWidget::onExportDeleteClicked);
    connect(importBtn, &QPushButton::clicked, this,
            &FileExplorerWidget::onImportClicked);
    connect(addToFavoritesBtn, &QPushButton::clicked, this,
            &FileExplorerWidget::onAddToFavoritesClicked);
}

void FileExplorerWidget::onSidebarItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)

    bool useAfc2 = item->data(0, Qt::UserRole).value<SidebarItemData>().first;
    QString path = item->data(0, Qt::UserRole).value<SidebarItemData>().second;

    // if (itemType == "try_install_afc2") {
    //     onTryInstallAFC2Clicked();
    //     return;
    // }

    switchToAFC(useAfc2);
    loadPath(path);
}

void FileExplorerWidget::onAddToFavoritesClicked()
{
    QString currentPath = "/";
    if (!history.isEmpty())
        currentPath = history.top();

    bool ok;
    QString alias = QInputDialog::getText(
        this, "Add to Favorites",
        "Enter alias for this location:", QLineEdit::Normal, currentPath, &ok);
    if (ok && !alias.isEmpty()) {
        saveFavoritePlace(currentPath, alias);
        refreshFavoritePlaces();
    }
}

void FileExplorerWidget::onTryInstallAFC2Clicked()
{
    qDebug() << "Clicked on try to install AFC2";
}

void FileExplorerWidget::switchToAFC(bool useAFC2)
{
    if (useAFC2 && device->afc2Client) {
        usingAFC2 = true;
        currentAfcClient = device->afc2Client;
    } else {
        usingAFC2 = false;
        currentAfcClient = device->afcClient;
    }
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
        QTreeWidgetItem *favoriteItem = new QTreeWidgetItem(favoritePlacesItem);
        favoriteItem->setText(0, alias);
        favoriteItem->setIcon(0, QIcon::fromTheme("folder-favorites"));
        favoriteItem->setData(
            0, Qt::UserRole, QVariant::fromValue(SidebarItemData(false, path)));
        favoriteItem->setData(0, Qt::UserRole + 1,
                              QVariant::fromValue(false)); // Default to AFC
    }
}

void FileExplorerWidget::saveFavoritePlace(const QString &path,
                                           const QString &alias)
{
    qDebug() << "Saving favorite place:" << alias << "->" << path;
    SettingsManager *settings = SettingsManager::sharedInstance();
    settings->saveFavoritePlace(path, alias);
}

void FileExplorerWidget::refreshFavoritePlaces()
{
    // Clear existing favorite items
    while (favoritePlacesItem->childCount() > 0) {
        delete favoritePlacesItem->takeChild(0);
    }

    // Reload favorite places
    loadFavoritePlaces();
}
