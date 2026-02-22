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

#include "infolabel.h"
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QMouseEvent>

InfoLabel::InfoLabel(const QString &text, const QString &textToCopy,
                     QWidget *parent)
    : QLabel(text, parent), m_originalText(text),
      m_textToCopy(!textToCopy.isEmpty() ? textToCopy : text)
{
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(m_style);
    m_restoreTimer = new QTimer(this);
    m_restoreTimer->setSingleShot(true);
    connect(m_restoreTimer, &QTimer::timeout, this,
            &InfoLabel::restoreOriginalText);
}

void InfoLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int originalWidth = width();

        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(m_textToCopy);

        // prevent layout shifts
        setMinimumWidth(originalWidth);
        setText("Copied!");
#ifdef WIN32
        setStyleSheet(QStringLiteral(
            "QLabel { color: #4CAF50; font-weight: bold; font-size: 14px; }"
            "QLabel:hover { background-color: rgba(255, 255, 255, 0.1); "
            "border-radius: 2px; }"));
#else
        setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; } "
                      "QLabel:hover { background-color: rgba(255, 255, 255, "
                      "0.1); border-radius: 2px; }");
#endif
        m_restoreTimer->start(1000); // Show "Copied!" for 1 second
    }
    QLabel::mousePressEvent(event);
}

void InfoLabel::restoreOriginalText()
{
    setText(m_originalText);
    setMinimumWidth(0);
    setStyleSheet(m_style);
}

void InfoLabel::setOriginalText(const QString &text) { m_originalText = text; }

void InfoLabel::setTextToCopy(const QString &textToCopy)
{
    m_textToCopy = textToCopy;
}