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

#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QStyleFactory>
#include <QtGlobal>
#include <stdlib.h>

#ifdef WIN32
#include "platform/windows/check_deps.h"
#endif
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#ifdef WIN32
    QString appPath = QCoreApplication::applicationDirPath();
    QString gstPluginPath =
        QDir::toNativeSeparators(appPath + "/gstreamer-1.0");
    QString gstPluginScannerPath = QDir::toNativeSeparators(
        appPath + "/gstreamer-1.0/libexec/gst-plugin-scanner.exe");

    const char *oldPath = getenv("PATH");
    QString newPath = appPath + ";" + QString(oldPath);
    qputenv("PATH", newPath.toUtf8());

    qputenv("GST_PLUGIN_PATH", gstPluginPath.toUtf8());
    qDebug() << "GST_PLUGIN_PATH=" << gstPluginPath;
    qputenv("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "no");
    qDebug() << "GST_REGISTRY_REUSE_PLUGIN_SCANNER=no";
    qputenv("GST_PLUGIN_SYSTEM_PATH", gstPluginPath.toUtf8());
    qDebug() << "GST_PLUGIN_SYSTEM_PATH=" << gstPluginPath;
    qputenv("GST_DEBUG", "GST_PLUGIN_LOADING:5");
    qDebug() << "GST_DEBUG=GST_PLUGIN_LOADING:5";
    qputenv("GST_PLUGIN_SCANNER_1_0", gstPluginScannerPath.toUtf8());
    qDebug() << "GST_PLUGIN_SCANNER_1_0=" << gstPluginScannerPath;
#endif
#ifdef __APPLE__
    QString appPath = QCoreApplication::applicationDirPath();
    QString frameworksPath =
        QDir::toNativeSeparators(appPath + "/../Frameworks");
    QString gstPluginPath =
        QDir::toNativeSeparators(frameworksPath + "/gstreamer");
    QString gstPluginScannerPath =
        QDir::toNativeSeparators(frameworksPath + "/gst-plugin-scanner");

    qputenv("DYLD_LIBRARY_PATH", frameworksPath.toUtf8());
    qputenv("GST_PLUGIN_SCANNER_1_0", gstPluginScannerPath.toUtf8());
    qputenv("GST_PLUGIN_SYSTEM_PATH_1_0", gstPluginPath.toUtf8());
#endif
    QCoreApplication::setOrganizationName("iDescriptor");
    QCoreApplication::setApplicationName("iDescriptor");
    QCoreApplication::setApplicationVersion(APP_VERSION);
#ifndef __APPLE__
    QApplication::setStyle(QStyleFactory::create("Fusion"));
#endif
    MainWindow *w = MainWindow::sharedInstance();
    w->show();
    return a.exec();
}
