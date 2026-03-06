#ifndef DIRPICKERLABEL_H
#define DIRPICKERLABEL_H

#include "iDescriptor-ui.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QWidget>

class DirPickerLabel : public QWidget
{
public:
    explicit DirPickerLabel(
        QWidget *parent = nullptr,
        const QString &calloutString = QString("Export to:"));
    QString getOutputDir() const { return m_outputDir; }

private:
    ZLabel *m_dirLabel;
    QPushButton *m_dirButton;
    QString m_outputDir =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
};

#endif // DIRPICKERLABEL_H
