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

#ifndef WINTOOLWIDGET_H
#define WINTOOLWIDGET_H

#include "../win_common.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class WinToolWidget : public QWidget
{
    Q_OBJECT

    HWND m_hwnd;
    int m_resize_border_width;

    QPushButton *m_min_btn;
    QPushButton *m_max_btn;
    QPushButton *m_close_btn;

    QWidget *m_content_widget;
    QVBoxLayout *m_content_layout;
    QWidget *m_titlebar_widget;
    QHBoxLayout *m_custom_titlebar_layout;
    QColor m_titleBarColor;

public:
    explicit WinToolWidget(QWidget *parent = nullptr);
    void setResizeBorderWidth(const int &resize_border_width);
    void setTitlebarHeight(const int &titlebar_height);
    QWidget &getTitlebarWidget();
    QHBoxLayout &getCustomTitlebarLayout();

private:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &event_type, void *message, long *result);
#else
    bool nativeEvent(const QByteArray &event_type, void *message,
                     qintptr *result);
#endif
    bool event(QEvent *evt);
    bool determineNonClickableWidgetUnderMouse(QLayout *layout, int x, int y);
    void propagateActiveStateInCustomTitlebar(QLayout *layout,
                                              bool active_state);
    void onScreenChanged(QScreen *screen);
    void onMinimizeButtonClicked();
    void onMaximizeButtonClicked();
    void onCloseButtonClicked();

protected:
    QWidget *contentWidget();
};

#endif // WINTOOLWIDGET_H
