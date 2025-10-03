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

AfcExplorerWidget::AfcExplorerWidget(afc_client_t afcClient,
                                     std::function<void()> onClientInvalidCb,
                                     iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_currentAfcClient(afcClient), m_device(device)
{
    // Initialize current AFC client to default
    m_currentAfcClient = afcClient;

    // Setup file explorer
    setupFileExplorer();

    // Main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    setLayout(mainLayout);
    mainLayout->addWidget(m_explorer);

    // Initialize
    m_history.push("/");
    loadPath("/");

    setupContextMenu();
    connect(SettingsManager::sharedInstance(),
            &SettingsManager::favoritePlacesChanged, this,
            &AfcExplorerWidget::refreshFavoritePlaces);
}

void AfcExplorerWidget::goBack()
{
    if (m_history.size() > 1) {
        m_history.pop();
        QString prevPath = m_history.top();
        loadPath(prevPath);
    }
}

void AfcExplorerWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    QVariant data = item->data(Qt::UserRole);
    bool isDir = data.toBool();
    QString name = item->text();

    // Use breadcrumb to get current path
    QString currPath = "/";
    if (!m_history.isEmpty())
        currPath = m_history.top();

    if (!currPath.endsWith("/"))
        currPath += "/";
    QString nextPath = currPath == "/" ? "/" + name : currPath + name;

    if (isDir) {
        m_history.push(nextPath);
        loadPath(nextPath);
    } else {
        auto *previewDialog = new MediaPreviewDialog(m_device, nextPath, this);
        previewDialog->setAttribute(Qt::WA_DeleteOnClose);
        previewDialog->show();
        // TODO: we need this ?
        emit fileSelected(nextPath);
    }
}

void AfcExplorerWidget::onBreadcrumbClicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    QString path = btn->property("fullPath").toString();
    // pathLabel removed, compare with m_history.top()
    if (!m_history.isEmpty() && path == m_history.top())
        return;
    m_history.push(path);
    loadPath(path);
}

void AfcExplorerWidget::updateBreadcrumb(const QString &path)
{
    // Remove old breadcrumb buttons
    QLayoutItem *child;
    while ((child = m_breadcrumbLayout->takeAt(0)) != nullptr) {
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
            &AfcExplorerWidget::onBreadcrumbClicked);
    m_breadcrumbLayout->addWidget(rootBtn);

    for (const QString &part : parts) {
        currPath += part;
        if (idx > 0) {
            QLabel *sep = new QLabel(" / ");
            m_breadcrumbLayout->addWidget(sep);
        }

        QPushButton *btn = new QPushButton(part);
        btn->setFlat(true);
        btn->setProperty("fullPath", currPath);
        connect(btn, &QPushButton::clicked, this,
                &AfcExplorerWidget::onBreadcrumbClicked);
        m_breadcrumbLayout->addWidget(btn);
        idx++;
    }
    m_breadcrumbLayout->addStretch();
}

void AfcExplorerWidget::loadPath(const QString &path)
{
    m_fileList->clear();
    // pathLabel->setText(path); // removed

    updateBreadcrumb(path);

    MediaFileTree tree =
        get_file_tree(m_currentAfcClient, m_device->device, path.toStdString());
    if (!tree.success) {
        m_fileList->addItem("Failed to load directory");
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
        m_fileList->addItem(item);
    }
}

void AfcExplorerWidget::setupContextMenu()
{
    m_fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileList, &QListWidget::customContextMenuRequested, this,
            &AfcExplorerWidget::onFileListContextMenu);
}

void AfcExplorerWidget::onFileListContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_fileList->itemAt(pos);
    if (!item)
        return;

    bool isDir = item->data(Qt::UserRole).toBool();
    if (isDir)
        return; // Only export files

    QMenu menu;
    QAction *exportAction = menu.addAction("Export");
    QAction *selectedAction =
        menu.exec(m_fileList->viewport()->mapToGlobal(pos));
    if (selectedAction == exportAction) {
        QList<QListWidgetItem *> selectedItems = m_fileList->selectedItems();
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

void AfcExplorerWidget::onExportClicked()
{
    QList<QListWidgetItem *> selectedItems = m_fileList->selectedItems();
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

void AfcExplorerWidget::onExportDeleteClicked()
{
    // Placeholder for future implementation
    QList<QListWidgetItem *> selectedItems = m_fileList->selectedItems();
    if (selectedItems.isEmpty())
        return;
    // TODO: Implement export & delete logic
    return;
}

void AfcExplorerWidget::exportSelectedFile(QListWidgetItem *item,
                                           const QString &directory)
{
    QString fileName = item->text();
    QString currPath = "/";
    if (!m_history.isEmpty())
        currPath = m_history.top();
    if (!currPath.endsWith("/"))
        currPath += "/";
    qDebug() << "Current path:" << currPath;
    QString devicePath = currPath == "/" ? "/" + fileName : currPath + fileName;
    qDebug() << "Exporting file:" << devicePath;

    // Save to selected directory
    QString savePath = directory + "/" + fileName;

    // Get device info and check connections
    qDebug() << "Using device index:" << m_device->udid.c_str();
    qDebug() << "Device UDID:" << QString::fromStdString(m_device->udid);
    qDebug() << "Device Product Type:"
             << QString::fromStdString(m_device->deviceInfo.productType);

    // Export file using the validated connections
    int result = export_file_to_path(m_currentAfcClient,
                                     devicePath.toStdString().c_str(),
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
int AfcExplorerWidget::export_file_to_path(afc_client_t afc,
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

void AfcExplorerWidget::onImportClicked()
{
    // TODO: check devices

    // Select one or more files to import
    QStringList fileNames = QFileDialog::getOpenFileNames(this, "Import Files");
    if (fileNames.isEmpty())
        return;

    // Use current breadcrumb directory as target
    QString currPath = "/";
    if (!m_history.isEmpty())
        currPath = m_history.top();
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
        int result = import_file_to_device(m_currentAfcClient,
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
int AfcExplorerWidget::import_file_to_device(afc_client_t afc,
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

void AfcExplorerWidget::setupFileExplorer()
{
    m_explorer = new QWidget();
    QVBoxLayout *explorerLayout = new QVBoxLayout(m_explorer);

    // Export/Import buttons layout
    QHBoxLayout *exportLayout = new QHBoxLayout();
    m_exportBtn = new QPushButton("Export");
    m_exportDeleteBtn = new QPushButton("Export & Delete");
    m_importBtn = new QPushButton("Import");
    m_addToFavoritesBtn = new QPushButton("Add to Favorites");
    exportLayout->addWidget(m_exportBtn);
    exportLayout->addWidget(m_exportDeleteBtn);
    exportLayout->addWidget(m_importBtn);
    exportLayout->addWidget(m_addToFavoritesBtn);
    exportLayout->addStretch();
    explorerLayout->addLayout(exportLayout);

    // Navigation layout (Back + Breadcrumb)
    QHBoxLayout *navLayout = new QHBoxLayout();
    m_backBtn = new QPushButton("Back");
    m_breadcrumbLayout = new QHBoxLayout();
    m_breadcrumbLayout->setSpacing(0);
    navLayout->addWidget(m_backBtn);
    navLayout->addLayout(m_breadcrumbLayout);
    navLayout->addStretch();
    explorerLayout->addLayout(navLayout);

    // File list
    m_fileList = new QListWidget();
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    explorerLayout->addWidget(m_fileList);

    // Connect buttons
    connect(m_backBtn, &QPushButton::clicked, this, &AfcExplorerWidget::goBack);
    connect(m_fileList, &QListWidget::itemDoubleClicked, this,
            &AfcExplorerWidget::onItemDoubleClicked);
    connect(m_exportBtn, &QPushButton::clicked, this,
            &AfcExplorerWidget::onExportClicked);
    connect(m_exportDeleteBtn, &QPushButton::clicked, this,
            &AfcExplorerWidget::onExportDeleteClicked);
    connect(m_importBtn, &QPushButton::clicked, this,
            &AfcExplorerWidget::onImportClicked);
    connect(m_addToFavoritesBtn, &QPushButton::clicked, this,
            &AfcExplorerWidget::onAddToFavoritesClicked);
}

void AfcExplorerWidget::onSidebarItemClicked(QTreeWidgetItem *item, int column)
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

void AfcExplorerWidget::onAddToFavoritesClicked()
{
    QString currentPath = "/";
    if (!m_history.isEmpty())
        currentPath = m_history.top();

    bool ok;
    QString alias = QInputDialog::getText(
        this, "Add to Favorites",
        "Enter alias for this location:", QLineEdit::Normal, currentPath, &ok);
    if (ok && !alias.isEmpty()) {
        saveFavoritePlace(currentPath, alias);
        refreshFavoritePlaces();
    }
}

void AfcExplorerWidget::onTryInstallAFC2Clicked()
{
    qDebug() << "Clicked on try to install AFC2";
}

void AfcExplorerWidget::switchToAFC(bool useAFC2)
{
    if (useAFC2 && m_device->afc2Client) {
        m_usingAFC2 = true;
        m_currentAfcClient = m_device->afc2Client;
    } else {
        m_usingAFC2 = false;
        m_currentAfcClient = m_device->afcClient;
    }
}

void AfcExplorerWidget::saveFavoritePlace(const QString &path,
                                          const QString &alias)
{
    qDebug() << "Saving favorite place:" << alias << "->" << path;
    SettingsManager *settings = SettingsManager::sharedInstance();
    settings->saveFavoritePlace(path, alias);
}

void AfcExplorerWidget::refreshFavoritePlaces()
{
    // // Clear existing favorite items
    // while (m_favoritePlacesItem->childCount() > 0) {
    //     delete m_favoritePlacesItem->takeChild(0);
    // }

    // // Reload favorite places
    // loadFavoritePlaces();
}
