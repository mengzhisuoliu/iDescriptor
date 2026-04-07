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

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "qprocessindicator.h"
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#ifdef __linux__
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QStandardPaths>
#endif

#include "service.h"

class DependencyItem : public QWidget
{
    Q_OBJECT

public:
    explicit DependencyItem(const QString &name, const QString &description,
                            bool optional = false, QWidget *parent = nullptr);
    void setInstalled(SERVICE_AVAILABILITY availability, bool isRequired);
    void setChecking(bool checking);
    void setInstalling(bool installing);
    void setActivating(bool activating);
    void setProgress(const QString &message);
    SERVICE_AVAILABILITY availability() const { return m_availability; }

signals:
    void installRequested(const QString &name,
                          SERVICE_AVAILABILITY availability);

private slots:
    void onInstallClicked();

private:
    QString m_name;
    QLabel *m_nameLabel;
    QLabel *m_descriptionLabel;
    QLabel *m_statusLabel;
    QPushButton *m_installButton;
    QProcessIndicator *m_processIndicator;
    SERVICE_AVAILABILITY m_availability = SERVICE_UNAVAILABLE;
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
    void addDependencyItem(const QString &name, const QString &description,
                           bool optional = false);

#ifdef WIN32
    void installBonjourRuntime();
#endif

#ifdef __linux__
    SERVICE_AVAILABILITY checkUdevRulesInstalled();
    SERVICE_AVAILABILITY checkAvahiDaemonRunning();
#endif

    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_itemsLayout;
    QPushButton *m_checkButton;
    ZIconWidget *m_toggleButton;
    QLabel *m_summaryLabel;
    QWidget *m_itemsWidget;
    bool m_isExpanded;

    QMap<QString, DependencyItem *> m_dependencyItems;
};

#endif // DIAGNOSE_WIDGET_H
