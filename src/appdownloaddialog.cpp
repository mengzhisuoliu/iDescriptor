/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "appdownloaddialog.h"
#include "BackDrop.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "libipatool-go.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

AppDownloadDialog::AppDownloadDialog(const QString &appName,
                                     const QString &bundleId,
                                     const QString &description,
                                     QWidget *parent)
    : AppDownloadBaseDialog(appName, bundleId, parent),
      m_outputDir(
          QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)),
      m_bundleId(bundleId)
{
    setWindowTitle("Download " + appName + " IPA");
    setModal(true);
    setFixedWidth(500);
    setContentsMargins(0, 0, 0, 0);

    m_loadingWidget = new ZLoadingWidget(true, this);
    layout()->addWidget(m_loadingWidget);
    QVBoxLayout *contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // m_bgLabel = new QLabel();
    // m_bgLabel->setScaledContents(true);
    // m_bgLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    // contentLayout->addWidget(m_bgLabel);

    QHBoxLayout *cardLayout = new QHBoxLayout();
    cardLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->addLayout(cardLayout);

    m_appIcon = new IDLoadingIconLabel();

    cardLayout->addWidget(m_appIcon);
    cardLayout->addSpacing(5);
    QVBoxLayout *textLayout = new QVBoxLayout();

    QLabel *nameLabel = new QLabel(appName);
    nameLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    nameLabel->setWordWrap(true);
    textLayout->addWidget(nameLabel);

    QLabel *bundleIdLabel = new QLabel(bundleId);
    bundleIdLabel->setStyleSheet("font-size: 12px; color: #666;");
    bundleIdLabel->setWordWrap(true);
    textLayout->addWidget(bundleIdLabel);

    textLayout->addSpacing(5);

    QLabel *descLabel = new QLabel(description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 14px; color: #666;");
    textLayout->addWidget(descLabel);

    cardLayout->addLayout(textLayout);
    QPointer<AppDownloadDialog> safeThis = this;
    fetchAppIconFromApple(
        m_manager, bundleId,
        [safeThis](const QPixmap &pixmap, const QJsonObject &appInfo) {
            if (auto dialog = safeThis.data()) {
                dialog->m_appIcon->setLoadedPixmap(pixmap);
                dialog->m_loadingWidget->stop(true);
            }
        });

    m_dirPickerLabel = new ZDirPickerLabel("Save to:");

    contentLayout->addWidget(m_dirPickerLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_actionButton = new QPushButton("Download IPA");
    m_actionButton->setDefault(true);
    connect(m_actionButton, &QPushButton::clicked, this,
            &AppDownloadDialog::onDownloadClicked);
    buttonLayout->addWidget(m_actionButton);

    QPushButton *cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    contentLayout->addLayout(buttonLayout);

    m_loadingWidget->setupContentWidget(contentLayout);
}

void AppDownloadDialog::onDownloadClicked()
{
    m_dirPickerLabel->disableDirSelection();
    m_actionButton->setEnabled(false);

    int buttonIndex = m_layout->indexOf(m_actionButton);
    layout()->removeWidget(m_actionButton);
    m_actionButton->deleteLater();
    qDebug() << "Starting download to" << m_outputDir;
    qDebug() << "Bundle ID:" << m_bundleId;
    startDownloadProcess(m_bundleId, m_outputDir, buttonIndex);
}
