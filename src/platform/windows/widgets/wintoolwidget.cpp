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

#include "wintoolwidget.h"
#include "../win_common.h"
#include <QApplication>
#include <QOperatingSystemVersion>
#include <QScreen>
#include <QStyle>
#include <QWindow>
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM

static const int TITLE_BAR_HEIGHT = 32;
// FIXME: custom title bar functionality
// doesnt work properly with Mica effetcs applied, need to investigate further
WinToolWidget::WinToolWidget(QWidget *parent)
    : QWidget(parent), m_min_btn{nullptr}, m_max_btn{nullptr},
      m_close_btn{nullptr}, m_resize_border_width{6}
{

    m_hwnd = (HWND)winId();
    setupWinWindow(this);

    QObject::connect(windowHandle(), &QWindow::screenChanged, this,
                     &WinToolWidget::onScreenChanged);

    // Add widget. (Initialize central widget)
    QWidget *entire_widget = this;
    setContentsMargins(0, 0, 0, 0);

    // Layout for entire widgets.
    QVBoxLayout *entire_layout = new QVBoxLayout(this);
    entire_widget->setLayout(entire_layout);
    entire_layout->setContentsMargins(0, 0, 0, 0);
    entire_layout->setSpacing(0);

    // Initialize title bar widget
    m_titlebar_widget = new QWidget(this);
    entire_layout->addWidget(m_titlebar_widget);
    m_titlebar_widget->setFixedHeight(35); // Default title bar height is 35
    m_titlebar_widget->setContentsMargins(0, 0, 0, 0);
    m_titlebar_widget->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);

    // Layout for title bar
    QHBoxLayout *titlebar_layout = new QHBoxLayout(this);
    m_titlebar_widget->setLayout(titlebar_layout);
    titlebar_layout->setContentsMargins(0, 0, 0, 0);
    titlebar_layout->setSpacing(0);

    QWidget *custom_titlebar_widget = new QWidget(this);
    titlebar_layout->addWidget(custom_titlebar_widget);
    custom_titlebar_widget->setContentsMargins(0, 0, 0, 0);
    custom_titlebar_widget->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);

    // Caption buttons – sized like Windows (46×32 for min/max, 46×32 for close)
    auto makeBtn = [&](const QString &icon, const QString &normalBg,
                       const QString &hoverBg,
                       const QString &pressBg) -> QPushButton * {
        auto *btn = new QPushButton(icon, this);
        btn->setFixedSize(46, TITLE_BAR_HEIGHT);
        btn->setFlat(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::ArrowCursor);
        btn->setStyleSheet(
            QString("QPushButton { background:%1; color:white; border:none;"
                    "              font-size:10px; font-family:'Segoe MDL2 "
                    "Assets','Segoe UI Symbol'; }"
                    "QPushButton:hover  { background:%2; }"
                    "QPushButton:pressed{ background:%3; }")
                .arg(normalBg, hoverBg, pressBg));
        return btn;
    };

    // U+E921 minimize, U+E922 restore/maximize, U+E8BB close  (Segoe MDL2
    // Assets)
    m_min_btn = makeBtn("\uE921", "transparent", "rgba(255,255,255,30)",
                        "rgba(255,255,255,20)");
    m_max_btn = makeBtn("\uE922", "transparent", "rgba(255,255,255,30)",
                        "rgba(255,255,255,20)");
    m_close_btn = makeBtn("\uE8BB", "transparent", "#C42B1C", "#9B1B0F");
    titlebar_layout->addWidget(m_min_btn);
    titlebar_layout->addWidget(m_max_btn);
    titlebar_layout->addWidget(m_close_btn);

    // Layout for title bar customization.
    m_custom_titlebar_layout = new QHBoxLayout(custom_titlebar_widget);
    custom_titlebar_widget->setLayout(m_custom_titlebar_layout);
    m_custom_titlebar_layout->setContentsMargins(0, 0, 0, 0);
    m_custom_titlebar_layout->setSpacing(0);
    m_custom_titlebar_layout->setAlignment(Qt::AlignLeft);

    QObject::connect(m_min_btn, &QPushButton::clicked, this,
                     &WinToolWidget::onMinimizeButtonClicked);
    QObject::connect(m_max_btn, &QPushButton::clicked, this,
                     &WinToolWidget::onMaximizeButtonClicked);
    QObject::connect(m_close_btn, &QPushButton::clicked, this,
                     &WinToolWidget::onCloseButtonClicked);
    entire_layout->setAlignment(titlebar_layout, Qt::AlignTop);

    m_content_widget = new QWidget(this);
    entire_layout->addWidget(m_content_widget);
    // m_content_layout = new QVBoxLayout(m_content_widget);
    m_content_widget->setContentsMargins(0, 0, 0, 0);
    m_content_widget->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Expanding);

    // Set default title bar palette.
    auto pal = m_titlebar_widget->palette();
    pal.setColor(QPalette::Window, QColor(30, 34, 39));
    m_titlebar_widget->setAutoFillBackground(true);
    m_titlebar_widget->setPalette(pal);

    // Set default content widget palette.
    pal = m_content_widget->palette();
    pal.setColor(QPalette::Window, QColor(35, 39, 46));
    m_content_widget->setAutoFillBackground(true);
    m_content_widget->setPalette(pal);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool WinToolWidget::nativeEvent(const QByteArray &event_type, void *message,
                                long *result)
#else
bool WinToolWidget::nativeEvent(const QByteArray &event_type, void *message,
                                qintptr *result)
#endif
{
    MSG *msg = (MSG *)message;

    switch (msg->message) {
    // Remove the default window frame by hooking the WM_NCCALCSIZE message.
    case WM_NCCALCSIZE: {
        if (msg->lParam) {
            WINDOWPLACEMENT wp;
            GetWindowPlacement(m_hwnd, &wp);

            if (wp.showCmd == SW_MAXIMIZE) {
                NCCALCSIZE_PARAMS *sz = (NCCALCSIZE_PARAMS *)msg->lParam;
                sz->rgrc[0].left += 8;
                sz->rgrc[0].top += 8;
                sz->rgrc[0].right -= 8;
                sz->rgrc[0].bottom -= 8;
            }
        }
        return true;
    }

    // Process the mouse when it is on the window border.
    case WM_NCHITTEST: {
        RECT winrect;
        GetWindowRect(msg->hwnd, &winrect);
        long x = GET_X_LPARAM(msg->lParam);
        long y = GET_Y_LPARAM(msg->lParam);
        long local_x = x - winrect.left;
        long local_y = y - winrect.top;

        if (x >= winrect.left && x < winrect.left + m_resize_border_width &&
            y < winrect.bottom && y >= winrect.bottom - m_resize_border_width) {
            *result = HTBOTTOMLEFT;
            return true;
        }

        if (x < winrect.right && x >= winrect.right - m_resize_border_width &&
            y < winrect.bottom && y >= winrect.bottom - m_resize_border_width) {
            *result = HTBOTTOMRIGHT;
            return true;
        }

        if (x >= winrect.left && x < winrect.left + m_resize_border_width &&
            y >= winrect.top && y < winrect.top + m_resize_border_width) {
            *result = HTTOPLEFT;
            return true;
        }

        if (x < winrect.right && x >= winrect.right - m_resize_border_width &&
            y >= winrect.top && y < winrect.top + m_resize_border_width) {
            *result = HTTOPRIGHT;
            return true;
        }

        if (x >= winrect.left && x < winrect.left + m_resize_border_width) {
            *result = HTLEFT;
            return true;
        }

        if (x < winrect.right && x >= winrect.right - m_resize_border_width) {
            *result = HTRIGHT;
            return true;
        }

        if (y < winrect.bottom && y >= winrect.bottom - m_resize_border_width) {
            *result = HTBOTTOM;
            return true;
        }

        if (y >= winrect.top && y < winrect.top + m_resize_border_width) {
            *result = HTTOP;
            return true;
        }

        // Check the area where the user can click to move the window.
        if (determineNonClickableWidgetUnderMouse(m_custom_titlebar_layout,
                                                  local_x, local_y)) {
            *result = HTCAPTION;
            return true;
        }

        *result = HTTRANSPARENT;
        break;
    }
    case WM_SIZE: {
        if (m_max_btn) {
            WINDOWPLACEMENT wp;
            GetWindowPlacement(m_hwnd, &wp);
            m_max_btn->setChecked(wp.showCmd == SW_MAXIMIZE ? true : false);
        }
        break;
    }
    default:
        break;
    }

    return false;
}

// This is used to change the `active` state of widgets in custom title bar.
bool WinToolWidget::event(QEvent *evt)
{
    switch (evt->type()) {
    case QEvent::WindowActivate: {
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
        m_close_btn->setStyleSheet(m_close_btn->styleSheet());
        m_min_btn->setStyleSheet(m_min_btn->styleSheet());
        m_max_btn->setStyleSheet(m_max_btn->styleSheet());
#endif
        propagateActiveStateInCustomTitlebar(m_custom_titlebar_layout, true);
        break;
    }

    case QEvent::WindowDeactivate: {
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
        m_close_btn->setStyleSheet(m_close_btn->styleSheet());
        m_min_btn->setStyleSheet(m_min_btn->styleSheet());
        m_max_btn->setStyleSheet(m_max_btn->styleSheet());
#endif
        propagateActiveStateInCustomTitlebar(m_custom_titlebar_layout, false);
        break;
    }

    default:
        break;
    }

    return QWidget::event(evt);
}

// Determine whether the current mouse coordinate is on the non-clickable widget
// or not using a recursive method.
bool WinToolWidget::determineNonClickableWidgetUnderMouse(QLayout *layout,
                                                          int x, int y)
{
    if (!layout->count() && layout->geometry().contains(x, y))
        return true;

    for (size_t i = 0; i < layout->count(); i++) {
        auto item = layout->itemAt(i)->widget();
        if (item) {
            if (item->geometry().contains(x, y))
                return !item->property("clickable widget").toBool();
        } else {
            auto child_layout = layout->itemAt(i)->layout();
            if (child_layout && child_layout->geometry().contains(x, y))
                return determineNonClickableWidgetUnderMouse(child_layout, x,
                                                             y);
        }
    }
    return false;
}

// Set `active' state using recursive method.
void WinToolWidget::propagateActiveStateInCustomTitlebar(QLayout *layout,
                                                         bool active_state)
{
    for (size_t i = 0; i < layout->count(); i++) {
        auto item = layout->itemAt(i)->widget();
        if (item) {
            item->setProperty("active", active_state);
            item->setStyleSheet(item->styleSheet());
        } else {
            auto child_layout = layout->itemAt(i)->layout();
            if (child_layout)
                propagateActiveStateInCustomTitlebar(child_layout,
                                                     active_state);
        }
    }
}

QWidget &WinToolWidget::getTitlebarWidget() { return *m_titlebar_widget; }

QHBoxLayout &WinToolWidget::getCustomTitlebarLayout()
{
    return *m_custom_titlebar_layout;
}

void WinToolWidget::setResizeBorderWidth(const int &resize_border_width)
{
    m_resize_border_width = resize_border_width;
}

void WinToolWidget::setTitlebarHeight(const int &titlebar_height)
{
    m_titlebar_widget->setFixedHeight(titlebar_height);
}

// Render again when frame is moved to another monitor.
void WinToolWidget::onScreenChanged(QScreen *screen)
{
    SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

void WinToolWidget::onMinimizeButtonClicked()
{
    SendMessage(m_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void WinToolWidget::onMaximizeButtonClicked()
{
    SendMessage(m_hwnd, WM_SYSCOMMAND,
                m_max_btn->isChecked() ? SC_MAXIMIZE : SC_RESTORE, 0);

    // Remove the hover state from the maximize button.
    m_max_btn->setAttribute(Qt::WA_UnderMouse, false);
}

void WinToolWidget::onCloseButtonClicked()
{
    SendMessage(m_hwnd, WM_CLOSE, 0, 0);
}

QWidget *WinToolWidget::contentWidget() { return m_content_widget; }
