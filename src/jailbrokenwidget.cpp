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

#include "jailbrokenwidget.h"
#include "appcontext.h"
#include "opensshterminalwidget.h"
#include "responsiveqlabel.h"

#ifdef __linux__
#include "core/services/avahi/avahi_service.h"
#else
#include "core/services/dnssd/dnssd_service.h"
#endif

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QApplication>
#include <QButtonGroup>
#include <QDebug>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

JailbrokenWidget::JailbrokenWidget(QWidget *parent) : QWidget{parent}
{
    QGridLayout *mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Define all the tools you want to display
    QList<JailbreakToolInfo> tools;
    tools.append({"Open SSH Terminal", "Connect to your device via SSH",
                  ":/resources/icons/TablerDatabaseExport.png"});
    tools.append({"More Tools Coming", "New features will be added soon",
                  ":/resources/icons/TablerDatabaseExport.png",
                  false}); // Disabled placeholder

    const int maxColumns = 3;
    for (int i = 0; i < tools.size(); ++i) {
        const auto &toolInfo = tools[i];
        ClickableWidget *toolWidget = createJailbreakTool(toolInfo);

        int row = i / maxColumns;
        int col = i % maxColumns;
        mainLayout->addWidget(toolWidget, row, col);
    }

    // Add a stretch to the last row and column to push everything to the
    // top-left
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);
    mainLayout->setColumnStretch(mainLayout->columnCount(), 1);
}

ClickableWidget *
JailbrokenWidget::createJailbreakTool(const JailbreakToolInfo &info)
{
    ClickableWidget *b = new ClickableWidget();
    b->setCursor(Qt::PointingHandCursor);
    b->setEnabled(info.enabled);

    // Use a theme-aware stylesheet for the background and hover effect
    b->setStyleSheet("ClickableWidget {"
                     "  border-radius: 8px;"
                     "  padding: 10px;"
                     "}");

    QVBoxLayout *layout = new QVBoxLayout(b);

    // Icon (using the theme-aware ZIcon pattern)
    // ZIconLabel *iconLabel = new ZIconLabel();
    ZIconLabel *iconLabel = new ZIconLabel(QIcon(), nullptr, 1.5, this);

    // iconLabel->setAlignment(Qt::AlignCenter);
    // ZIcon toolIcon(QIcon(info.iconPath));

    // auto updateIcon = [b, iconLabel, toolIcon]() {
    //     iconLabel->setPixmap(
    //         toolIcon.getThemedPixmap(QSize(45, 45), b->palette()));
    // };
    // updateIcon();
    // connect(qApp, &QApplication::paletteChanged, b, updateIcon);

    // Title
    QLabel *titleLabel = new QLabel(info.title);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    // Description (using a theme-aware palette color)
    QLabel *descLabel = new QLabel(info.description);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("font-size: 12px;");

    layout->addWidget(iconLabel, 0, Qt::AlignCenter);
    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);

    // TODO: Connect the clicked signal to a slot
    if (info.title == "Open SSH Terminal") {
        iconLabel->setIcon(QIcon(":/resources/icons/BxBxsTerminal.png"));

        connect(b, &ClickableWidget::clicked, this, [this]() {
            if (m_sshTerminalWidget) {
                m_sshTerminalWidget->raise();
                m_sshTerminalWidget->activateWindow();
                return;
            }
            m_sshTerminalWidget = new OpenSSHTerminalWidget();
            m_sshTerminalWidget->setAttribute(Qt::WA_DeleteOnClose);
            m_sshTerminalWidget->show();
            m_sshTerminalWidget->raise();
            m_sshTerminalWidget->activateWindow();
            connect(m_sshTerminalWidget, &QObject::destroyed, this,
                    [this]() { m_sshTerminalWidget = nullptr; });
        });
    } else if (info.title == "More Tools Coming") {
        iconLabel->setIcon(
            QIcon(":/resources/icons/IconParkTwotoneMoreTwo.png"));
    }
    iconLabel->setIconSizeMultiplier(2);
    return b;
}

JailbrokenWidget::~JailbrokenWidget() {}