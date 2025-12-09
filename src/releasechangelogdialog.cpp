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

#include "releasechangelogdialog.h"
#include "iDescriptor.h"
#include "settingsmanager.h"
#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

ReleaseChangelogDialog::ReleaseChangelogDialog(QJsonDocument data,
                                               QWidget *parent)
    : QDialog(parent)
{
    setupUI(data);
}

ReleaseChangelogDialog::~ReleaseChangelogDialog() {}

void ReleaseChangelogDialog::setupUI(const QJsonDocument &data)
{
    setWindowTitle("iDescriptor - Release Changelog");
    setModal(true);
    setMinimumSize(500, 250);
    resize(600, 300);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(15);

    m_titleLabel =
        new QLabel(QString("iDescriptor has been updated to v") + APP_VERSION);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(
        "font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    m_mainLayout->addWidget(m_titleLabel);

    QString description = "Failed to load changelog data.";
    QJsonArray dataArr = data.array();
    if (!dataArr.isEmpty()) {
        for (const QJsonValue &releaseVal : dataArr) {
            QJsonObject releaseObj = releaseVal.toObject();
            if (!releaseObj.isEmpty()) {
                QString tagName = releaseObj.value("tag_name").toString();

                if (tagName.isEmpty()) {
                    continue;
                }
                if (tagName == QString("v") + APP_VERSION) {
                    if (releaseObj.value("body").isUndefined())
                        break;
                    description = releaseObj.value("body").toString();
                    break;
                }
            }
        }
    }

    m_descriptionLabel = new QLabel(description);
    m_descriptionLabel->setAlignment(Qt::AlignCenter);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setStyleSheet("font-size: 14px; margin: 10px;");
    m_mainLayout->addWidget(m_descriptionLabel);

    m_mainLayout->addStretch();

    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    m_skipButton = new QPushButton("Ok, Thanks!");
    m_skipButton->setFixedHeight(40);

    m_donateButton = new QPushButton("Donate");
    m_donateButton->setDefault(true);
    m_donateButton->setFixedHeight(40);

    buttonsLayout->addWidget(m_skipButton);
    buttonsLayout->addWidget(m_donateButton);

    m_mainLayout->addLayout(buttonsLayout, Qt::AlignCenter);

    connect(m_donateButton, &QPushButton::clicked, this,
            &ReleaseChangelogDialog::onDonateClicked);
    connect(m_skipButton, &QPushButton::clicked, this,
            &ReleaseChangelogDialog::onSkipButtonClicked);
}

void ReleaseChangelogDialog::onDonateClicked()
{
    QDesktopServices::openUrl(QUrl(DONATE_URL));
    accept();
}

void ReleaseChangelogDialog::onSkipButtonClicked() { accept(); }
