#ifndef EXPORTALBUM_H
#define EXPORTALBUM_H

#include "appcontext.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "iomanagerclient.h"
#include "qprocessindicator.h"
#include "zloadingwidget.h"
#include <QDialog>
#include <QMessageBox>
#include <QTimer>
#include <QWidget>
#include <QtConcurrent>
#include <atomic>

class ExportAlbum : public QDialog
{
    Q_OBJECT
public:
    explicit ExportAlbum(const std::shared_ptr<iDescriptorDevice> device,
                         const QStringList &paths, QWidget *parent = nullptr);

private:
    ZLoadingWidget *m_loadingWidget;
    const std::shared_ptr<iDescriptorDevice> m_device;
    QLabel *m_infoLabel;
    size_t m_listCount;
    QList<QString> m_exportItems;
    ZDirPickerLabel *m_dirPickerLabel;
    QLabel *m_totalSizeExportLabel;
    QProcessIndicator *m_loadingIndicator = nullptr;
    std::atomic<uint64_t> m_totalExportSize{0};
    std::atomic<bool> m_exiting{false};
    void getTotalPhotoCount(const QStringList &paths);
    void updateInfoLabel(size_t photoCount);
    // startExport(const QStringList &paths, const QString &exportDir);
    void startExport();
    void calculateTotalExportSize();
};

#endif // EXPORTALBUM_H
