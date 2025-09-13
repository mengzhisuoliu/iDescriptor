#include "gallerywidget.h"
#include "fileexportdialog.h"
#include "iDescriptor.h"
#include "mediapreviewdialog.h"
#include "photoexportmanager.h"
#include "photomodel.h"
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

QByteArray read_afc_file_to_byte_array(afc_client_t afcClient, const char *path)
{
    uint64_t fd_handle = 0;
    afc_error_t fd_err =
        afc_file_open(afcClient, path, AFC_FOPEN_RDONLY, &fd_handle);

    if (fd_err != AFC_E_SUCCESS) {
        qDebug() << "Could not open file" << path;
        return QByteArray();
    }

    // TODO: is this necessary
    char **info = NULL;
    afc_get_file_info(afcClient, path, &info);
    uint64_t fileSize = 0;
    if (info) {
        for (int i = 0; info[i]; i += 2) {
            if (strcmp(info[i], "st_size") == 0) {
                fileSize = std::stoull(info[i + 1]);
                break;
            }
        }
        afc_dictionary_free(info);
    }

    if (fileSize == 0) {
        afc_file_close(afcClient, fd_handle);
        return QByteArray();
    }

    QByteArray buffer;

    buffer.resize(fileSize);
    uint32_t bytesRead = 0;
    afc_file_read(afcClient, fd_handle, buffer.data(), buffer.size(),
                  &bytesRead);

    if (bytesRead != fileSize) {
        qDebug() << "AFC Error: Read mismatch for file" << path;
        return QByteArray(); // Read failed
    }

    return buffer;
};

void GalleryWidget::load()
{
    if (m_loaded)
        return;
    m_loaded = true;

    setupUI();
}

GalleryWidget::GalleryWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget{parent}, m_device(device), m_model(nullptr),
      m_exportManager(nullptr)
{
    // Initialize export manager
    m_exportManager = new PhotoExportManager(this);

    // Widget setup is done in load() method when gallery tab is activated
}

void GalleryWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    // Setup controls at the top
    setupControlsLayout();

    // Create list view
    m_listView = new QListView(this);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setWrapping(true);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setIconSize(QSize(120, 120));
    m_listView->setSpacing(10);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Create and set model
    m_model = new PhotoModel(m_device, this);
    m_listView->setModel(m_model);

    // Add to main layout
    m_mainLayout->addWidget(m_listView);
    setLayout(m_mainLayout);

    // Add progress widget after main layout is set
    // m_mainLayout->insertWidget(
    //     1, m_progressWidget); // Insert between controls and list view

    // Connect double-click to open preview dialog
    connect(m_listView, &QListView::doubleClicked, this,
            [this](const QModelIndex &index) {
                if (!index.isValid())
                    return;

                QString filePath =
                    m_model->data(index, Qt::UserRole).toString();
                if (filePath.isEmpty())
                    return;

                qDebug() << "Opening preview for" << filePath;
                auto *previewDialog =
                    new MediaPreviewDialog(m_device, filePath, this);
                previewDialog->setAttribute(Qt::WA_DeleteOnClose);
                previewDialog->show();
            });

    // Update export button states based on selection
    connect(m_listView->selectionModel(),
            &QItemSelectionModel::selectionChanged, this, [this]() {
                bool hasSelection =
                    m_listView->selectionModel()->hasSelection();
                m_exportSelectedButton->setEnabled(hasSelection);
            });
}

void GalleryWidget::setupControlsLayout()
{
    m_controlsLayout = new QHBoxLayout();
    m_controlsLayout->setSpacing(15);

    // Sort order combo box
    QLabel *sortLabel = new QLabel("Sort:");
    sortLabel->setStyleSheet("font-weight: bold; color: #555;");
    m_sortComboBox = new QComboBox();
    m_sortComboBox->addItem("Newest First",
                            static_cast<int>(PhotoModel::NewestFirst));
    m_sortComboBox->addItem("Oldest First",
                            static_cast<int>(PhotoModel::OldestFirst));
    m_sortComboBox->setCurrentIndex(0); // Default to Newest First
    m_sortComboBox->setStyleSheet("QComboBox { "
                                  "  padding: 5px 10px; "
                                  "  border: 1px solid #ccc; "
                                  "  border-radius: 4px; "
                                  "  background-color: white; "
                                  "  min-width: 100px; "
                                  "} "
                                  "QComboBox:hover { "
                                  "  border-color: #0078d4; "
                                  "} "
                                  "QComboBox::drop-down { "
                                  "  border: none; "
                                  "  width: 20px; "
                                  "} "
                                  "QComboBox::down-arrow { "
                                  "  width: 12px; "
                                  "  height: 12px; "
                                  "}");

    // Filter combo box
    QLabel *filterLabel = new QLabel("Filter:");
    filterLabel->setStyleSheet("font-weight: bold; color: #555;");
    m_filterComboBox = new QComboBox();
    m_filterComboBox->addItem("All Media", static_cast<int>(PhotoModel::All));
    m_filterComboBox->addItem("Images Only",
                              static_cast<int>(PhotoModel::ImagesOnly));
    m_filterComboBox->addItem("Videos Only",
                              static_cast<int>(PhotoModel::VideosOnly));
    m_filterComboBox->setCurrentIndex(0); // Default to All
    m_filterComboBox->setStyleSheet(m_sortComboBox->styleSheet());

    // Export buttons
    m_exportSelectedButton = new QPushButton("Export Selected");
    m_exportSelectedButton->setEnabled(false); // Initially disabled
    m_exportSelectedButton->setStyleSheet("QPushButton { "
                                          "  background-color: #0078d4; "
                                          "  color: white; "
                                          "  border: none; "
                                          "  padding: 8px 16px; "
                                          "  border-radius: 4px; "
                                          "  font-weight: bold; "
                                          "} "
                                          "QPushButton:hover:enabled { "
                                          "  background-color: #106ebe; "
                                          "} "
                                          "QPushButton:pressed:enabled { "
                                          "  background-color: #005a9e; "
                                          "} "
                                          "QPushButton:disabled { "
                                          "  background-color: #ccc; "
                                          "  color: #888; "
                                          "}");

    m_exportAllButton = new QPushButton("Export All");
    m_exportAllButton->setStyleSheet("QPushButton { "
                                     "  background-color: #28a745; "
                                     "  color: white; "
                                     "  border: none; "
                                     "  padding: 8px 16px; "
                                     "  border-radius: 4px; "
                                     "  font-weight: bold; "
                                     "} "
                                     "QPushButton:hover { "
                                     "  background-color: #218838; "
                                     "} "
                                     "QPushButton:pressed { "
                                     "  background-color: #1e7e34; "
                                     "}");

    // Connect signals
    connect(m_sortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GalleryWidget::onSortOrderChanged);
    connect(m_filterComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GalleryWidget::onFilterChanged);
    connect(m_exportSelectedButton, &QPushButton::clicked, this,
            &GalleryWidget::onExportSelected);
    connect(m_exportAllButton, &QPushButton::clicked, this,
            &GalleryWidget::onExportAll);

    // Add widgets to layout
    m_controlsLayout->addWidget(sortLabel);
    m_controlsLayout->addWidget(m_sortComboBox);
    m_controlsLayout->addWidget(filterLabel);
    m_controlsLayout->addWidget(m_filterComboBox);
    m_controlsLayout->addStretch(); // Push export buttons to the right
    m_controlsLayout->addWidget(m_exportSelectedButton);
    m_controlsLayout->addWidget(m_exportAllButton);

    // Create a frame to contain the controls
    QWidget *controlsWidget = new QWidget();
    controlsWidget->setLayout(m_controlsLayout);
    controlsWidget->setStyleSheet("QWidget { "
                                  "  background-color: #f8f9fa; "
                                  "  border: 1px solid #dee2e6; "
                                  "  border-radius: 6px; "
                                  "  padding: 10px; "
                                  "}");

    m_mainLayout->addWidget(controlsWidget);
}

void GalleryWidget::onSortOrderChanged()
{
    if (!m_model)
        return;

    int sortValue = m_sortComboBox->currentData().toInt();
    PhotoModel::SortOrder order = static_cast<PhotoModel::SortOrder>(sortValue);
    m_model->setSortOrder(order);

    qDebug() << "Sort order changed to:"
             << (order == PhotoModel::NewestFirst ? "Newest First"
                                                  : "Oldest First");
}

void GalleryWidget::onFilterChanged()
{
    if (!m_model)
        return;

    int filterValue = m_filterComboBox->currentData().toInt();
    PhotoModel::FilterType filter =
        static_cast<PhotoModel::FilterType>(filterValue);
    m_model->setFilterType(filter);

    QString filterName = m_filterComboBox->currentText();
    qDebug() << "Filter changed to:" << filterName;
}

void GalleryWidget::onExportSelected()
{
    if (!m_model || !m_listView->selectionModel()->hasSelection()) {
        QMessageBox::information(this, "No Selection",
                                 "Please select one or more items to export.");
        return;
    }

    if (m_exportManager->isExporting()) {
        QMessageBox::information(this, "Export in Progress",
                                 "An export operation is already in progress. "
                                 "Please wait for it to complete.");
        return;
    }

    QModelIndexList selectedIndexes =
        m_listView->selectionModel()->selectedIndexes();
    QStringList filePaths = m_model->getSelectedFilePaths(selectedIndexes);

    if (filePaths.isEmpty()) {
        QMessageBox::warning(this, "Export Error",
                             "No valid files selected for export.");
        return;
    }

    QString exportDir = selectExportDirectory();
    if (exportDir.isEmpty()) {
        return; // User cancelled directory selection
    }

    qDebug() << "Starting export of selected files:" << filePaths.size()
             << "items to" << exportDir;

    // Create export dialog and connect signals
    auto *exportDialog = new FileExportDialog(this);

    // Connect PhotoExportManager signals to FileExportDialog
    connect(m_exportManager, &PhotoExportManager::exportStarted, exportDialog,
            &FileExportDialog::onExportStarted);
    connect(m_exportManager, &PhotoExportManager::exportProgress, exportDialog,
            &FileExportDialog::onExportProgress);
    connect(m_exportManager, &PhotoExportManager::exportFinished, exportDialog,
            &FileExportDialog::onExportFinished);
    connect(m_exportManager, &PhotoExportManager::exportCancelled, exportDialog,
            &FileExportDialog::onExportCancelled);

    // Connect cancel signal from dialog to export manager
    connect(exportDialog, &FileExportDialog::cancelRequested, m_exportManager,
            &PhotoExportManager::cancelExport);

    // Start the export
    m_exportManager->exportFiles(m_device, filePaths, exportDir);
}

void GalleryWidget::onExportAll()
{
    if (!m_model)
        return;

    if (m_exportManager->isExporting()) {
        QMessageBox::information(this, "Export in Progress",
                                 "An export operation is already in progress. "
                                 "Please wait for it to complete.");
        return;
    }

    QStringList filePaths = m_model->getFilteredFilePaths();

    if (filePaths.isEmpty()) {
        QMessageBox::information(
            this, "No Items",
            "There are no items to export with the current filter.");
        return;
    }

    QString message =
        QString("Export all %1 items currently shown?").arg(filePaths.size());
    int reply = QMessageBox::question(this, "Export All", message,
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString exportDir = selectExportDirectory();
    if (exportDir.isEmpty()) {
        return; // User cancelled directory selection
    }

    qDebug() << "Starting export of all filtered files:" << filePaths.size()
             << "items to" << exportDir;

    // Create export dialog and connect signals
    auto *exportDialog = new FileExportDialog(this);

    // Connect PhotoExportManager signals to FileExportDialog
    connect(m_exportManager, &PhotoExportManager::exportStarted, exportDialog,
            &FileExportDialog::onExportStarted);
    connect(m_exportManager, &PhotoExportManager::exportProgress, exportDialog,
            &FileExportDialog::onExportProgress);
    connect(m_exportManager, &PhotoExportManager::exportFinished, exportDialog,
            &FileExportDialog::onExportFinished);
    connect(m_exportManager, &PhotoExportManager::exportCancelled, exportDialog,
            &FileExportDialog::onExportCancelled);

    // Connect cancel signal from dialog to export manager
    connect(exportDialog, &FileExportDialog::cancelRequested, m_exportManager,
            &PhotoExportManager::cancelExport);

    // Start the export
    m_exportManager->exportFiles(m_device, filePaths, exportDir);
}

QString GalleryWidget::selectExportDirectory()
{
    QString defaultDir =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    QString selectedDir = QFileDialog::getExistingDirectory(
        this, "Select Export Directory", defaultDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    return selectedDir;
}
