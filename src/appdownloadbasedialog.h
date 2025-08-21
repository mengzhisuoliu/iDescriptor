#ifndef APPDOWNLOADBASEDIALOG_H
#define APPDOWNLOADBASEDIALOG_H

#include <QDialog>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

class AppDownloadBaseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AppDownloadBaseDialog(const QString &appName,
                                   QWidget *parent = nullptr);

public slots:
    void updateProgressBar(int percentage);

protected:
    void startDownloadProcess(const QString &bundleId,
                              const QString &workingDir, int index);
    void checkDownloadProgress(const QString &logFilePath,
                               const QString &appName,
                               const QString &outputDir);
    void addProgressBar(int index);
    QProgressBar *m_progressBar;
    QTimer *m_progressTimer;
    QProcess *m_downloadProcess;
    QString m_appName;
    QPushButton *m_actionButton;
    QVBoxLayout *m_layout;
};

#endif // APPDOWNLOADBASEDIALOG_H
