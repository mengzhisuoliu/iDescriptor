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

#include "privateinfolabel.h"

PrivateInfoLabel::PrivateInfoLabel(const QString &fullText, QWidget *parent)
    : QWidget(parent), m_fullText(fullText), m_isVisible(false)
{
    m_maskedText = getMaskedText(fullText);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    m_textLabel = new InfoLabel(m_maskedText, this);
    layout->addWidget(m_textLabel);

    m_toggleButton = new ZIconWidget(
        QIcon(":/resources/icons/ClarityEyeHideLine.png"), "Show", 1.0, this);
    m_toggleButton->setIconSize(QSize(20, 20));
    connect(m_toggleButton, &ZIconWidget::clicked, this,
            &PrivateInfoLabel::toggleVisibility);
    layout->addWidget(m_toggleButton);

    layout->addStretch();
}

QString PrivateInfoLabel::getMaskedText(const QString &text)
{
    if (text.length() <= 4) {
        return QString("*").repeated(text.length());
    }
    // Show first 4 characters, hide the rest
    return text.left(4) + QString("*").repeated(text.length() - 4);
}

void PrivateInfoLabel::toggleVisibility()
{
    m_isVisible = !m_isVisible;
    if (m_isVisible) {
        m_textLabel->setText(m_fullText);
        m_textLabel->setOriginalText(m_fullText);
        m_toggleButton->setIcon(QIcon(":/resources/icons/ClarityEyeLine.png"));
        m_toggleButton->setToolTip("Hide");
    } else {
        m_textLabel->setText(m_maskedText);
        m_textLabel->setOriginalText(m_fullText);
        m_toggleButton->setIcon(
            QIcon(":/resources/icons/ClarityEyeHideLine.png"));
        m_toggleButton->setToolTip("Show");
    }
}
