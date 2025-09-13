#ifndef FILEEXPORTDIALOG_H
#define FILEEXPORTDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QProgressBar;
class QLabel;
class QPushButton;
class QVBoxLayout;
QT_END_NAMESPACE

class FileExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileExportDialog(QWidget *parent = nullptr);
    ~FileExportDialog() override;

public slots:
    void onExportStarted(int totalFiles);
    void onExportProgress(int completed, int total,
                          const QString &currentFileName);
    void onExportFinished(int successful, int failed);
    void onExportCancelled();

signals:
    void cancelRequested();

private:
    void setupUI();
    void showCompletionMessage(int successful, int failed);

    // UI components
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QLabel *m_fileLabel;
    QPushButton *m_cancelButton;
    QVBoxLayout *m_layout;
};

#endif // FILEEXPORTDIALOG_H