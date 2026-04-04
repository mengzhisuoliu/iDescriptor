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

    m_loadingWidget = new ZLoadingWidget(false, this);
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

    m_watcher = new QFutureWatcher<ScanResult>(this);

    connect(m_watcher, &QFutureWatcher<ScanResult>::finished, this, [this]() {
        ScanResult result = m_watcher->result();
        qDebug() << "Total photo count:" << result.count << "with"
                 << (result.ok ? 0 : 1) << "errors";

        if (result.ok) {
            m_exportItems = std::move(result.items);
            updateInfoLabel(result.count);
            calculateTotalExportSize();
            m_loadingWidget->stop();
        } else {
            QMessageBox::warning(
                nullptr, "Error",
                "Failed to read directory: cannot export album(s)");
            reject();
        }

        m_watcher->deleteLater();
        m_watcher = nullptr;
    });

    // FIXME: if a dir returns empty, it could be an error or just an empty
    // dir, we should check that
    m_watcher->setFuture(QtConcurrent::run([device = m_device,
                                            paths]() -> ScanResult {
        ScanResult res{true, 0, {}};

        for (const QString &path : paths) {
            QList<QString> items = device->afc_backend->list_files_flat(path);

            if (items.isEmpty()) {
                res.ok = false;
                continue;
            }

            for (const QString &item : items) {
                if (item.isEmpty())
                    continue;
                res.items.append(item);
            }
            res.count += items.size();
        }
        qDebug() << "[m_watcher] Finished scanning albums, total items found:"
                 << res.count;
        return res;
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

ExportAlbum::~ExportAlbum()
{
    if (m_watcher) {
        qDebug() << "Cancelling ongoing scan in ExportAlbum destructor";
        m_watcher->cancel();
        // m_watcher->waitForFinished();
        m_watcher->deleteLater();
    }
}