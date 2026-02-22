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

#include "welcomewidget.h"
#include "diagnosewidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "networkdevicestoconnectwidget.h"
#include "responsiveqlabel.h"
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPalette>
#include <QUrl>

WelcomeWidget::WelcomeWidget(QWidget *parent) : QWidget(parent) { setupUI(); }

void WelcomeWidget::setupUI()
{
    // Main layout with proper spacing and margins
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 10, 0, 0);
    m_mainLayout->setSpacing(0);

    // Add top stretch
    m_mainLayout->addStretch(1);

    // Welcome title
    m_titleLabel = createStyledLabel("Welcome to iDescriptor", 28, true);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_titleLabel);

    // Subtitle
    m_subtitleLabel = createStyledLabel("Open-Source & Free", 10, false);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    QPalette palette = m_subtitleLabel->palette();
    m_mainLayout->addWidget(m_subtitleLabel);

    QHBoxLayout *imageAndWirelessDevicesLayout = new QHBoxLayout();

    m_imageLabel = new QLabel();
    m_imageLabel->setPixmap(QPixmap(":/resources/connect.png"));
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageLabel->setMinimumSize(QSize(400, 0));
    m_imageLabel->setStyleSheet("background: transparent; border: none;");
    m_imageLabel->setAlignment(Qt::AlignCenter);

    imageAndWirelessDevicesLayout->addWidget(m_imageLabel, 0, Qt::AlignHCenter);
    NetworkDevicesToConnectWidget *networkDevicesWidget =
        new NetworkDevicesToConnectWidget();
    imageAndWirelessDevicesLayout->addWidget(networkDevicesWidget);

    m_mainLayout->addLayout(imageAndWirelessDevicesLayout);
    m_mainLayout->addSpacing(10);

    m_instructionLabel =
        createStyledLabel("Connect an iDevice to get started", 14, false);
    m_instructionLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_instructionLabel);
    m_mainLayout->addSpacing(10);

    // GitHub link
    m_githubLabel = createStyledLabel("Found an issue? Report it on GitHub", 12,
                                      false, COLOR_HYPERLINK);
    m_githubLabel->setWordWrap(false);
    m_githubLabel->setMaximumWidth(m_imageLabel->sizeHint().width());
    m_githubLabel->setCursor(Qt::PointingHandCursor);
    connect(m_githubLabel, &ZLabel::clicked, this,
            []() { QDesktopServices::openUrl(QUrl(REPO_URL)); });

    QPalette githubPalette = m_githubLabel->palette();
    githubPalette.setColor(QPalette::WindowText,
                           COLOR_HYPERLINK); // Apple blue
    m_githubLabel->setPalette(githubPalette);

    m_mainLayout->addWidget(m_githubLabel, 0, Qt::AlignCenter);

    // no additional deps needed on macOS
#ifndef __APPLE__
    DiagnoseWidget *diagnoseWidget = new DiagnoseWidget();
    m_mainLayout->addWidget(diagnoseWidget);
#endif

    m_mainLayout->addStretch(1);
}

// FIXME: color param is only respected in Windows build
ZLabel *WelcomeWidget::createStyledLabel(const QString &text, int fontSize,
                                         bool isBold, QColor color)
{
    ZLabel *label = new ZLabel(text);

#ifndef WIN32
    QFont font = label->font();
    if (fontSize > 0) {
        font.setPointSize(fontSize);
    }
    if (isBold) {
        font.setWeight(QFont::Medium);
    }

    label->setFont(font);
    label->setWordWrap(true);
#else
    label->setStyleSheet(mergeStyles(
        label,
        QString("QLabel {"
                "  font-size: %1px;"
                "  font-weight: %2;"
                "%3"
                "}")
            .arg(fontSize > 0 ? QString::number(fontSize) : "inherit")
            .arg(isBold ? "bold" : "normal")
            //    FIXME: handle this better
            .arg(color != Qt::black ? QString("color: %1;").arg(color.name())
                                    : "")));
#endif

    return label;
}
