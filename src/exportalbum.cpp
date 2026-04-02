#include "exportalbum.h"

ExportAlbum::ExportAlbum(const std::shared_ptr<iDescriptorDevice> device,
                         const QStringList &paths, QWidget *parent)
    : QDialog(parent), m_device(device), m_listCount(paths.size())
{
    setWindowTitle("Export Album");
    setMaximumSize(600, 400);
#ifdef WIN32
    setupWinWindow(this);
#endif

    m_loadingWidget = new ZLoadingWidget(true, this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_loadingWidget);

    getTotalPhotoCount(paths);
    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this](const QString &udid) {
                if (udid == m_device->udid) {
                    m_exiting = true;
                    QTimer::singleShot(0, this, [this]() { close(); });
                }
            });

    QWidget *contentWidget = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);

    m_infoLabel = new QLabel(this);
    contentLayout->addWidget(m_infoLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *cancelButton = new QPushButton("Cancel", this);
    QPushButton *exportButton = new QPushButton("Export", this);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(cancelButton);
    m_dirPickerLabel = new ZDirPickerLabel();

    contentLayout->addWidget(m_dirPickerLabel);

    QHBoxLayout *sizeLayout = new QHBoxLayout();

    m_totalSizeExportLabel = new QLabel("Total size to export: 0 MB", this);
    sizeLayout->addWidget(m_totalSizeExportLabel);

    m_loadingIndicator = new QProcessIndicator(this);
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(32, 16);
    sizeLayout->addWidget(m_loadingIndicator);
    sizeLayout->addStretch();

    contentLayout->addLayout(sizeLayout);
    contentLayout->addLayout(buttonLayout);

    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        m_exiting = true;
        QTimer::singleShot(0, this, [this]() { close(); });
    });
    connect(exportButton, &QPushButton::clicked, this, [this, exportButton]() {
        m_exiting = true;
        exportButton->setEnabled(false);
        QTimer::singleShot(0, this, [this]() {
            startExport();
            accept();
        });
    });

    m_loadingWidget->setupContentWidget(contentWidget);

    connect(this, &QDialog::finished, this, [this](int) {
        m_exiting = true;
        deleteLater();
    });
}

void ExportAlbum::getTotalPhotoCount(const QStringList &paths)
{
    QFutureWatcher<std::pair<bool, size_t>> *watcher =
        new QFutureWatcher<std::pair<bool, size_t>>(this);

    connect(watcher, &QFutureWatcher<std::pair<bool, size_t>>::finished, this,
            [this, watcher]() {
                std::pair<bool, size_t> result = watcher->result();
                qDebug() << "Total photo count:" << result.second << "with"
                         << (result.first ? 0 : 1) << "errors";

                if (result.first) {
                    updateInfoLabel(result.second);
                    calculateTotalExportSize();
                    m_loadingWidget->stop();
                } else {
                    QMessageBox::warning(
                        nullptr, "Error",
                        "Failed to read directory: cannot export album(s)");
                    reject();
                }
            });

    watcher->setFuture(QtConcurrent::run([this, paths]() {
        size_t count = 0;
        bool errorOccurred = false;
        for (const QString &path : paths) {
            QList<QString> items = m_device->afc_backend->list_files_flat(path);

            if (items.isEmpty()) {
                errorOccurred = true;
            } else {
                for (const QString &item : items) {
                    if (item.isEmpty()) {
                        continue;
                    }
                    m_exportItems.append(item);
                }
                count += items.size();
            }
        }
        return std::make_pair(!errorOccurred, count);
    }));
}

void ExportAlbum::updateInfoLabel(size_t photoCount)
{
    m_infoLabel->setText(QString("Are you sure you want to export %1 album(s) "
                                 "with %2 photo(s)/video(s) ?")
                             .arg(m_listCount)
                             .arg(photoCount));
}

void ExportAlbum::startExport()
{
    IOManagerClient::sharedInstance()->startExport(
        m_device, m_exportItems, m_dirPickerLabel->getOutputDir(),
        "Exporting Album(s)");
}

void ExportAlbum::calculateTotalExportSize()
{
    m_totalExportSize = 0;
    m_loadingIndicator->start();

    auto timer = new QTimer(this);
    timer->setInterval(500);

    connect(timer, &QTimer::timeout, this, [this]() {
        m_totalSizeExportLabel->setText(
            QString("Total size to export: %1")
                .arg(iDescriptor::Utils::formatSize(m_totalExportSize.load())));
    });

    timer->start();

    QThreadPool::globalInstance()->start([this, timer]() {
        for (const QString &item : m_exportItems) {
            if (m_exiting.load()) {
                return;
            }

            int size = m_device->afc_backend->get_file_size(item);
            this->m_totalExportSize += size;
        }

        QMetaObject::invokeMethod(
            this,
            [this, timer]() {
                if (m_exiting.load()) {
                    return;
                }
                timer->stop();
                timer->deleteLater();
                this->m_totalSizeExportLabel->setText(
                    QString("Total size to export: %1")
                        .arg(iDescriptor::Utils::formatSize(
                            this->m_totalExportSize.load())));
                this->m_loadingIndicator->stop();
                this->m_loadingIndicator->hide();
            },
            Qt::QueuedConnection);
    });
}