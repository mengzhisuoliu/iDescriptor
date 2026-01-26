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

#ifndef DIAGNOSE_WIDGET_H
#define DIAGNOSE_WIDGET_H

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "qprocessindicator.h"

class DependencyItem : public QWidget
{
    Q_OBJECT

public:
    explicit DependencyItem(const QString &name, const QString &description,
                            QWidget *parent = nullptr);
    void setInstalled(bool installed);
    void setChecking(bool checking);
    void setInstalling(bool installing);
    void setProgress(const QString &message);

signals:
    void installRequested(const QString &name);

private slots:
    void onInstallClicked();

private:
    QString m_name;
    QLabel *m_nameLabel;
    QLabel *m_descriptionLabel;
    QLabel *m_statusLabel;
    QPushButton *m_installButton;
    QProcessIndicator *m_processIndicator;
};

class DiagnoseWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnoseWidget(QWidget *parent = nullptr);

public slots:
    void checkDependencies(bool autoExpand = true);

private slots:
    void onInstallRequested(const QString &name);
    void onToggleExpand();

private:
    void setupUI();
    void addDependencyItem(const QString &name, const QString &description);

#ifdef WIN32
    void installBonjourRuntime();
#endif

#ifdef __linux__
    bool checkUdevRulesInstalled();
    bool checkAvahiDaemonRunning();
#endif

    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_itemsLayout;
    QPushButton *m_checkButton;
    QPushButton *m_toggleButton;
    QLabel *m_summaryLabel;
    QWidget *m_itemsWidget;
    bool m_isExpanded;

    QList<DependencyItem *> m_dependencyItems;
};

#endif // DIAGNOSE_WIDGET_H
