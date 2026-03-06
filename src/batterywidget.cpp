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

// https://github.com/p-dobrzynski-dev/QtCustomWidgets/blob/master/batterywidget.cpp
#include "batterywidget.h"

#include <QApplication>
#include <QDebug>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

BatteryWidget::BatteryWidget(float value, bool isCharging, QWidget *parent)
    : QWidget(parent), m_value(value), m_isCharging(isCharging)
{
    setMinimumSize(30, 30);
    setMaximumSize(40, 40);
}

void BatteryWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void BatteryWidget::resizeEvent(QResizeEvent *)
{
    widgetFrame = this->rect();

    widgetFrame.setSize(QSize(widgetFrame.width(), widgetFrame.height() / 2));
    widgetFrame.moveTop(widgetFrame.center().y());

    float scaleValue = 0.95;
    QSizeF mainBatteryFrameSize =
        QSizeF(widgetFrame.width() * scaleValue, widgetFrame.height());
    mainBatteryFrame.setSize(mainBatteryFrameSize);

    mainBatteryFrame.moveTopLeft(widgetFrame.topLeft());

    QSizeF tipBatteryFrameSize =
        QSizeF(widgetFrame.width() / 3, widgetFrame.height() / 2);
    tipBatteryFrame.setSize(tipBatteryFrameSize);

    QPointF tipBatteryFramePoint =
        QPointF(widgetFrame.topRight().x() - tipBatteryFrameSize.width(),
                widgetFrame.topRight().y() + tipBatteryFrameSize.height() / 2);
    tipBatteryFrame.moveTopLeft(tipBatteryFramePoint);

    float batteryLevelOffset = mainBatteryFrame.height() / 20;
    QSizeF batteryLevelFrameSize =
        QSizeF(mainBatteryFrame.width() - 2 * batteryLevelOffset,
               mainBatteryFrame.height() - 2 * batteryLevelOffset);
    batteryLevelFrame.setSize(batteryLevelFrameSize);

    QPointF batteryFramePoint =
        QPointF(mainBatteryFrame.topLeft().x() + batteryLevelOffset,
                mainBatteryFrame.topLeft().y() + batteryLevelOffset);
    batteryLevelFrame.moveTopLeft(batteryFramePoint);
}

void BatteryWidget::setValue(float newValue)
{
    m_value = newValue;
    this->update();
}

float BatteryWidget::getValue() const { return m_value; }

void BatteryWidget::setChargingState(bool state)
{
    m_isCharging = state;
    this->update();
    this->repaint();
}

void BatteryWidget::updateContext(bool isCharging, float newValue)
{
    m_isCharging = isCharging;
    m_value = newValue;
    this->update();
    this->repaint();
}

bool BatteryWidget::getChargingState() { return m_isCharging; }

void BatteryWidget::paintEvent(QPaintEvent *)
{

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen = QPen(Qt::red);
    QBrush brush = QBrush(Qt::white);

    painter.setPen(pen);

    float widgetCorner = widgetFrame.height() / 15;

    QPainterPath FramePath;
    FramePath.setFillRule(Qt::WindingFill);
    FramePath.addRoundedRect(mainBatteryFrame, widgetCorner, widgetCorner);
    FramePath.addRoundedRect(tipBatteryFrame, widgetCorner, widgetCorner);
    FramePath = FramePath.simplified();

    pen.setColor(Qt::gray);
    pen.setWidth(widgetFrame.width() / 75);
    painter.setPen(pen);
    painter.drawPath(FramePath);

    painter.setBrush(QBrush(QColor("#44bd32")));
    painter.setPen(Qt::NoPen);
    QRectF batteryLevelRect = QRectF();
    QSize batterySizeRect =
        QSize(batteryLevelFrame.width() * m_value / m_maxValue,
              batteryLevelFrame.height());
    batteryLevelRect.setSize(batterySizeRect);
    batteryLevelRect.moveTo(batteryLevelFrame.topLeft());
    painter.drawRoundedRect(batteryLevelRect, widgetCorner, widgetCorner);

    pen.setColor(palette().color(QPalette::Text));
    painter.setPen(pen);
#ifndef WIN32
    QFont textFont = QFont();
    textFont.setWeight(QFont::Bold);
#else
    QFont textFont = QFont("Segoe UI Variable Small", 12);
#endif
    textFont.setPixelSize(widgetFrame.height() / 1.65);
    painter.setFont(textFont);
    QFontMetrics fm(textFont);
    QString percentageLevelString = QString("%1%").arg(m_value);
    float textWidth = fm.horizontalAdvance(percentageLevelString);
    float textHeight = fm.height();

    QPointF textPosition = QPointF(widgetFrame.center().x() - textWidth / 2,
                                   widgetFrame.center().y() + textHeight / 3);
    painter.drawText(textPosition, percentageLevelString);
}