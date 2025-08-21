#include "appdownloaddialog.h"
#include "clickablelabel.h"
#include "libipatool-go.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AppDownloadDialog::AppDownloadDialog(const QString &appName,
                                     const QString &bundleId,
                                     const QString &description,
                                     QWidget *parent)
    : AppDownloadBaseDialog(appName, parent),
      m_outputDir(QDir::homePath().append("/Downloads")), m_bundleId(bundleId)
{
    setWindowTitle("Download " + appName + " IPA");
    setModal(true);
    // setFixedSize(500, 270);
    setFixedWidth(500);

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(this->layout());

    QLabel *descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 14px; color: #666;");
    layout->insertWidget(1, descLabel);

    // Directory selection UI
    QHBoxLayout *dirLayout = new QHBoxLayout();
    QLabel *dirTextLabel = new QLabel("Save to:");
    dirTextLabel->setStyleSheet("font-size: 14px; color: #333;");
    dirLayout->addWidget(dirTextLabel);

    m_dirLabel = new ClickableLabel(this);
    m_dirLabel->setText(m_outputDir);
    m_dirLabel->setStyleSheet("font-size: 14px; color: #007AFF;");
    connect(m_dirLabel, &ClickableLabel::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir));
    });
    m_dirLabel->setCursor(Qt::PointingHandCursor);
    dirLayout->addWidget(m_dirLabel, 1);

    m_dirButton = new QPushButton("Choose...");
    m_dirButton->setStyleSheet("font-size: 14px; padding: 4px 12px;");
    connect(m_dirButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, "Select Directory to Save IPA", m_outputDir);
        if (!dir.isEmpty()) {
            m_outputDir = dir;
            m_dirLabel->setText(m_outputDir);
        }
    });
    dirLayout->addWidget(m_dirButton);

    layout->insertLayout(2, dirLayout);

    m_actionButton = new QPushButton("Download IPA");
    m_actionButton->setFixedHeight(40);
    m_actionButton->setStyleSheet(
        "background-color: #34C759; color: white; border: none; border-radius: "
        "6px; font-size: 16px; font-weight: bold;");
    connect(m_actionButton, &QPushButton::clicked, this,
            &AppDownloadDialog::onDownloadClicked);
    layout->addWidget(m_actionButton);

    QPushButton *cancelButton = new QPushButton("Cancel");
    cancelButton->setFixedHeight(40);
    cancelButton->setStyleSheet(
        "background-color: #f0f0f0; color: #333; border: 1px solid #ddd; "
        "border-radius: 6px; font-size: 16px;");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(cancelButton);
}

void AppDownloadDialog::onDownloadClicked()
{
    // Disable directory selection once download starts
    m_dirButton->setEnabled(false);
    m_actionButton->setEnabled(false);

    int buttonIndex = m_layout->indexOf(m_actionButton);
    layout()->removeWidget(m_actionButton);
    m_actionButton->deleteLater();

    startDownloadProcess(m_bundleId, m_outputDir, buttonIndex);
}
