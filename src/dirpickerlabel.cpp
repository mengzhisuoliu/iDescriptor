#include "dirpickerlabel.h"

DirPickerLabel::DirPickerLabel(QWidget *parent, const QString &calloutString)
    : QWidget{parent}
{
    // Directory selection UI
    QHBoxLayout *dirLayout = new QHBoxLayout();
    QLabel *dirTextLabel = new QLabel(calloutString);
    dirTextLabel->setStyleSheet("font-size: 14px;");
    dirLayout->addWidget(dirTextLabel);

    m_dirLabel = new ZLabel(this);
    m_dirLabel->setText(m_outputDir);
    m_dirLabel->setStyleSheet("font-size: 14px; color: #007AFF;");
    connect(m_dirLabel, &ZLabel::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir));
    });
    m_dirLabel->setCursor(Qt::PointingHandCursor);
    dirLayout->addWidget(m_dirLabel, 1);

    m_dirButton = new QPushButton("Choose...");
    // m_dirButton->setStyleSheet("font-size: 14px; padding: 4px 12px;");
    connect(m_dirButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, "Select Directory to Save IPA", m_outputDir);
        if (!dir.isEmpty()) {
            m_outputDir = dir;
            m_dirLabel->setText(m_outputDir);
        }
    });
    dirLayout->addWidget(m_dirButton);

    setLayout(dirLayout);
}
