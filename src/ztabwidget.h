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

#ifndef ZTABWIDGET_H
#define ZTABWIDGET_H

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

class ZTab : public QPushButton
{
    Q_OBJECT

public:
    explicit ZTab(const QString &text, QWidget *parent = nullptr);
    void setIcon(const QIcon &icon);
};

class ZTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ZTabWidget(QWidget *parent = nullptr);
    void finalizeStyles();
    ZTab *addTab(QWidget *widget, const QString &label);
    void setCurrentIndex(int index);
    int currentIndex() const;
    QWidget *widget(int index) const;

signals:
    void currentChanged(int index);

private slots:
    void onTabClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QHBoxLayout *m_tabLayout;
    QVBoxLayout *m_mainLayout;
    QWidget *m_tabBar;
    QStackedWidget *m_stackedWidget;
    QButtonGroup *m_buttonGroup;
    QWidget *m_glider;
    QAbstractAnimation *m_gliderAnimation = nullptr;
    QList<ZTab *> m_tabs;
    QList<QWidget *> m_widgets;
    int m_currentIndex;

    void setupGlider();
    void animateGlider(int index, bool onResize = false);
    void animateWidget(QWidget *widget);
    void updateTabStyles();
};

#endif // ZTABWIDGET_H