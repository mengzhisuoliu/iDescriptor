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

#include "diagnosewidget.h"
#ifdef WIN32
#include "platform/windows/check_deps.h"
#include <archive.h>
#include <archive_entry.h>
#endif
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

DependencyItem::DependencyItem(const QString &name, const QString &description,
                               QWidget *parent)
    : QWidget(parent), m_name(name)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *infoLayout = new QHBoxLayout();

    m_nameLabel = new QLabel(name);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 1);
    m_nameLabel->setFont(nameFont);

    m_descriptionLabel = new QLabel(QString("(%1)").arg(description));
    m_descriptionLabel->setWordWrap(false);

    infoLayout->addWidget(m_nameLabel);
    infoLayout->addWidget(m_descriptionLabel);

    // Middle - status
    m_statusLabel = new QLabel("Checking...");
    m_statusLabel->setMinimumWidth(100);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // Right side - actions
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);

    m_installButton = new QPushButton("Install");
    m_installButton->setMinimumWidth(80);
    m_installButton->setVisible(false);
    connect(m_installButton, &QPushButton::clicked, this,
            &DependencyItem::onInstallClicked);

    m_processIndicator = new QProcessIndicator();
    m_processIndicator->setType(QProcessIndicator::line_rotate);
    m_processIndicator->setFixedSize(24, 24);
    m_processIndicator->setVisible(false);

    actionLayout->addWidget(m_processIndicator);
    actionLayout->addWidget(m_installButton);

    layout->addLayout(infoLayout);
    layout->addStretch();
    layout->addWidget(m_statusLabel);
    layout->addLayout(actionLayout);
}

void DependencyItem::setInstalled(bool installed)
{
    setChecking(false);

    if (installed) {
        if (m_name == "Avahi Daemon") {
            m_statusLabel->setText("✓ Activated");
        } else {
            m_statusLabel->setText("✓ Installed");
        }
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
        m_installButton->setVisible(false);
    } else {
        if (m_name == "Avahi Daemon") {
            m_statusLabel->setText("✗ Not activated");
            m_installButton->setText("Enable");
        } else {
            m_statusLabel->setText("✗ Not Installed");
            m_installButton->setText("Install");
        }
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
        m_installButton->setVisible(true);
    }
}

void DependencyItem::setChecking(bool checking)
{
    if (checking) {
        m_statusLabel->setText("Checking...");
        m_statusLabel->setStyleSheet("color: gray;");
        m_installButton->setVisible(false);
        m_processIndicator->setVisible(false);
        m_processIndicator->stop();
    }
}

void DependencyItem::setInstalling(bool installing)
{
    if (installing) {
        m_statusLabel->setText("Installing...");
        m_statusLabel->setStyleSheet("color: gray;");
        m_installButton->setVisible(false);
        m_processIndicator->setVisible(true);
        m_processIndicator->start();
    } else {
        m_processIndicator->stop();
        m_processIndicator->setVisible(false);
    }
}

void DependencyItem::setProgress(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet("color: gray;");
}

void DependencyItem::onInstallClicked() { emit installRequested(m_name); }

DiagnoseWidget::DiagnoseWidget(QWidget *parent)
    : QWidget(parent), m_isExpanded(false)
{
    setupUI();

#ifdef WIN32
    addDependencyItem("Bonjour Service",
                      "Required for AirPlay and network service discovery");
    addDependencyItem("Apple Mobile Device Support",
                      "Required for iOS device communication");
    addDependencyItem("WinFsp", "Required for mounting your device as a drive");
#endif

#ifdef __linux__
#ifdef ENABLE_RECOVERY_DEVICE_SUPPORT
    addDependencyItem("USB Device Permissions",
                      "Required for recovery devices (udev rules)");
#endif
    addDependencyItem("Avahi Daemon",
                      "Required for Airplay, device discovery and more");
#endif

    // Auto-check on startup
    QTimer::singleShot(100, this, [this]() { checkDependencies(); });
}

void DiagnoseWidget::setupUI()
{
    setObjectName("diagnoseWidget");
    setContentsMargins(20, 2, 20, 0);
    setAutoFillBackground(true);
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(5);

    // Title and summary
    QLabel *titleLabel = new QLabel("Dependency Check");
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);

    m_summaryLabel = new QLabel("Checking system dependencies...");

    // Check button
    m_checkButton = new QPushButton("Refresh Check(s)");
    m_checkButton->setMaximumWidth(m_checkButton->sizeHint().width());
    connect(m_checkButton, &QPushButton::clicked, this,
            [this]() { checkDependencies(false); });

    // Toggle button
    m_toggleButton = new QPushButton("▼");
    m_toggleButton->setFixedSize(24, 24);
    m_toggleButton->setCheckable(true);
    connect(m_toggleButton, &QPushButton::clicked, this,
            &DiagnoseWidget::onToggleExpand);

    m_itemsWidget = new QWidget();
    m_itemsLayout = new QVBoxLayout(m_itemsWidget);
    m_itemsLayout->setSpacing(10);
    m_itemsLayout->addStretch();
    m_itemsWidget->setVisible(m_isExpanded);

    // Layout assembly
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_checkButton);
    headerLayout->addWidget(m_toggleButton);

    m_mainLayout->addLayout(headerLayout);
    m_mainLayout->addWidget(m_summaryLabel);
    m_mainLayout->addWidget(m_itemsWidget);
}

void DiagnoseWidget::addDependencyItem(const QString &name,
                                       const QString &description)
{
    DependencyItem *item = new DependencyItem(name, description);
    item->setProperty("name", name);
    connect(item, &DependencyItem::installRequested, this,
            &DiagnoseWidget::onInstallRequested);

    m_dependencyItems.append(item);

    // Insert before the stretch
    m_itemsLayout->insertWidget(m_itemsLayout->count() - 1, item);
}

void DiagnoseWidget::checkDependencies(bool autoExpand)
{
    m_summaryLabel->setText("Checking system dependencies...");
    m_checkButton->setEnabled(false);

    for (DependencyItem *item : m_dependencyItems) {
        item->setChecking(true);
    }

    QTimer::singleShot(500, [this, autoExpand]() {
        int installedCount = 0;
        int totalCount = m_dependencyItems.size();

        for (DependencyItem *item : m_dependencyItems) {
            bool installed = false;
            QString itemName = item->property("name").toString();

#ifdef WIN32
            if (itemName == "Bonjour Service") {
                installed = IsBonjourServiceInstalled();
            } else if (itemName == "Apple Mobile Device Support") {
                installed = IsAppleMobileDeviceSupportInstalled();
            } else if (itemName == "WinFsp") {
                installed = IsWinFspInstalled();
            }
#endif

#ifdef __linux__
            if (itemName == "USB Device Permissions") {
                installed = checkUdevRulesInstalled();
            } else if (itemName == "Avahi Daemon") {
                installed = checkAvahiDaemonRunning();
            }
#endif

            item->setInstalled(installed);
            if (installed)
                installedCount++;
        }

        if (installedCount == totalCount) {
            m_summaryLabel->setText(
                QString("All dependencies are installed/activated (%1/%2)")
                    .arg(installedCount)
                    .arg(totalCount));
            m_summaryLabel->setStyleSheet("color: green; font-weight: bold;");
            if (m_isExpanded && autoExpand) {
                onToggleExpand();
            }
        } else {
            m_summaryLabel->setText(
                QString("Missing dependencies (%1/%2 installed)")
                    .arg(installedCount)
                    .arg(totalCount));
            m_summaryLabel->setStyleSheet("color: red; font-weight: bold;");
            if (!m_isExpanded && autoExpand) {
                onToggleExpand();
            }
        }

        m_checkButton->setEnabled(true);
    });
}

void DiagnoseWidget::onInstallRequested(const QString &name)
{
#ifdef WIN32
    if (name == "Bonjour Service") {
        installBonjourRuntime();
        return;
    }

    if (name == "Apple Mobile Device Support") {
        DependencyItem *itemToInstall = nullptr;
        for (DependencyItem *item : m_dependencyItems) {
            if (item->property("name").toString() == name) {
                itemToInstall = item;
                break;
            }
        }

        if (!itemToInstall)
            return;

        itemToInstall->setInstalling(true);

        QString scriptPath = QCoreApplication::applicationDirPath() +
                             "/install-apple-drivers.ps1";

        QProcess *installProcess = new QProcess(this);
        connect(
            installProcess, &QProcess::finished, this,
            [this, installProcess,
             itemToInstall](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    QString errorOutput =
                        installProcess->readAllStandardError();
                    if (errorOutput.isEmpty()) {
                        errorOutput = installProcess->readAllStandardOutput();
                    }
                    QMessageBox::warning(
                        this, "Installation Failed",
                        "Failed to launch the installation script. This "
                        "might be a "
                        "permissions issue or an internal error.\n\n"
                        "Details: " +
                            errorOutput.trimmed());
                    checkDependencies(false); // Revert UI on failure
                } else {
                    // FIXME: we need to track process completion
                    QMessageBox::information(
                        this, "Installation Started",
                        "The installation process has been started.\n"
                        "Please approve the administrator prompt (UAC) if it "
                        "appears.\n"
                        "After installation, please re-run the dependency "
                        "check");

                    itemToInstall->setInstalling(false);
                }
                installProcess->deleteLater();
            });

        // Correctly launch powershell.exe elevated, and pass the script to it.
        // The -Wait parameter is removed as it does not work with -Verb RunAs.
        QString command =
            QString("Start-Process -FilePath powershell.exe -Verb RunAs "
                    "-ArgumentList '-NoProfile -ExecutionPolicy Bypass -File "
                    "\"%1\"'")
                .arg(scriptPath);

        QStringList args;
        args << "-NoProfile"
             << "-ExecutionPolicy"
             << "Bypass"
             << "-Command" << command;

        installProcess->start("powershell.exe", args);
        return;
    }

    if (name == "WinFsp") {
        DependencyItem *itemToInstall = nullptr;
        for (DependencyItem *item : m_dependencyItems) {
            if (item->property("name").toString() == name) {
                itemToInstall = item;
                break;
            }
        }

        if (!itemToInstall)
            return;

        itemToInstall->setInstalling(true);

        QString scriptPath = QCoreApplication::applicationDirPath() +
                             "/install-win-fsp.silent.bat";

        QProcess *installProcess = new QProcess(this);
        connect(
            installProcess, &QProcess::finished, this,
            [this, installProcess](int exitCode,
                                   QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    QMessageBox::warning(
                        this, "Installation Failed",
                        "The installation script failed to run correctly. "
                        "This might be because the action was cancelled or an "
                        "error occurred.\n\nPlease try again.");
                }
                checkDependencies(false);
                installProcess->deleteLater();
            });

        QStringList args;
        args << "-NoProfile"
             << "-ExecutionPolicy"
             << "Bypass"
             << "-Command"
             << QString("Start-Process -FilePath \"%1\" -Verb RunAs -Wait;")
                    .arg(scriptPath);

        installProcess->start("powershell.exe", args);
    }
#endif

#ifdef __linux__
    if (name == "USB Device Permissions") {
        DependencyItem *itemToInstall = nullptr;
        for (DependencyItem *item : m_dependencyItems) {
            if (item->property("name").toString() == name) {
                itemToInstall = item;
                break;
            }
        }

        if (!itemToInstall)
            return;

        itemToInstall->setInstalling(true);

        QString userName = qEnvironmentVariable("USER");
        if (userName.isEmpty()) {
            userName = qEnvironmentVariable("LOGNAME");
        }

        if (userName.isEmpty()) {
            QMessageBox::critical(
                this, "Error",
                "Failed to determine the current user. Cannot "
                "proceed with the installation.");
            itemToInstall->setInstalling(false);
            return;
        }

        // Create a temporary script to set up udev rules
        QString scriptPath = QDir::temp().filePath("setup-idevice-udev.sh");
        QFile scriptFile(scriptPath);

        if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, "Error",
                                  "Failed to create installation script");
            itemToInstall->setInstalling(false);
            return;
        }

        // FIXME: maybe, can be handled better
        QTextStream out(&scriptFile);
        out << "#!/bin/bash\n";
        out << "set -e\n\n";
        out << "USERNAME=$1\n\n";
        out << "if [ -z \"$USERNAME\" ]; then\n";
        out << "    echo \"Error: Username not provided.\" >&2\n";
        out << "    exit 1\n";
        out << "fi\n\n";
        out << "# Create udev rules file\n";
        out << "echo 'SUBSYSTEM==\"usb\", ATTR{idVendor}==\"05ac\", "
               "MODE=\"0666\", GROUP=\"idevice\"' | tee "
               "/etc/udev/rules.d/99-idevice.rules > "
               "/dev/null\n\n";
        out << "# Create idevice group if it doesn't exist\n";
        out << "if ! getent group idevice > /dev/null 2>&1; then\n";
        out << "    groupadd idevice\n";
        out << "fi\n\n";
        out << "# Add current user to idevice group\n";
        out << "usermod -aG idevice \"$USERNAME\"\n\n";
        out << "# Reload udev rules\n";
        out << "udevadm control --reload-rules\n";
        out << "udevadm trigger\n\n";
        out << "echo 'USB device permissions configured successfully!'\n";
        out << "echo 'Note: You may need to log out and log back in for group "
               "changes to take effect.'\n";
        scriptFile.close();

        // Make script executable
        QFile::setPermissions(scriptPath, QFileDevice::ReadOwner |
                                              QFileDevice::WriteOwner |
                                              QFileDevice::ExeOwner);

        QProcess *installProcess = new QProcess(this);
        connect(
            installProcess, &QProcess::finished, this,
            [this, installProcess, itemToInstall,
             scriptPath](int exitCode, QProcess::ExitStatus exitStatus) {
                // Clean up temporary script
                QFile::remove(scriptPath);

                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    QString errorOutput =
                        installProcess->readAllStandardError();
                    if (errorOutput.isEmpty()) {
                        errorOutput = installProcess->readAllStandardOutput();
                    }
                    QMessageBox::warning(
                        this, "Installation Failed",
                        "Failed to configure USB device permissions. "
                        "This might be because the action was cancelled or an "
                        "error occurred.\n\nDetails: " +
                            errorOutput.trimmed());
                    checkDependencies(false);
                } else {
                    QMessageBox::information(
                        this, "Installation Complete",
                        "USB device permissions have been configured.\n\n"
                        "Note: You may need to log out and log back in for or "
                        "even restart for changes to take full effect.");
                    checkDependencies(false);
                }
                itemToInstall->setInstalling(false);
                installProcess->deleteLater();
            });

        QStringList args;
        args << scriptPath << userName;
        installProcess->start("pkexec", args);
    }

    if (name == "Avahi Daemon") {
        DependencyItem *itemToInstall = nullptr;
        for (DependencyItem *item : m_dependencyItems) {
            if (item->property("name").toString() == name) {
                itemToInstall = item;
                break;
            }
        }

        if (!itemToInstall)
            return;

        itemToInstall->setInstalling(true);

        QProcess *installProcess = new QProcess(this);
        connect(
            installProcess, &QProcess::finished, this,
            [this, installProcess,
             itemToInstall](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    QString errorOutput =
                        installProcess->readAllStandardError();
                    if (errorOutput.isEmpty()) {
                        errorOutput = installProcess->readAllStandardOutput();
                    }
                    QMessageBox::warning(
                        this, "Error",
                        "Failed to enable Avahi daemon. "
                        "This might be because the action was cancelled or an "
                        "error occurred.\n\nDetails: " +
                            errorOutput.trimmed());
                    checkDependencies(false);
                } else {
                    checkDependencies(false);
                }
                itemToInstall->setInstalling(false);
                installProcess->deleteLater();
            });

        QStringList args;
        args << "systemctl"
             << "enable"
             << "--now"
             << "avahi-daemon.service";
        installProcess->start("pkexec", args);
    }

#endif
}

#ifdef __linux__
bool DiagnoseWidget::checkUdevRulesInstalled()
{
    // Check if udev rules file exists
    QFile rulesFile("/etc/udev/rules.d/99-idevice.rules");
    if (!rulesFile.exists()) {
        return false;
    }

    // Check if the file contains the correct rule
    if (!rulesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&rulesFile);
    QString content = in.readAll();
    rulesFile.close();

    // Check for the essential parts of the rule
    bool hasUsbSubsystem = content.contains("SUBSYSTEM==\"usb\"");
    bool hasAppleVendor = content.contains("ATTR{idVendor}==\"05ac\"");
    bool hasMode = content.contains("MODE=\"0666\"");

    if (!hasUsbSubsystem || !hasAppleVendor || !hasMode) {
        return false;
    }

    // Check if current user is in the idevice group
    QProcess groupsProcess;
    groupsProcess.start("groups");
    groupsProcess.waitForFinished(3000);

    if (groupsProcess.exitCode() != 0) {
        // If we can't check groups, consider it not installed
        return false;
    }

    QString groupsOutput =
        QString::fromUtf8(groupsProcess.readAllStandardOutput()).trimmed();
    QStringList groups =
        groupsOutput.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    bool isInIdeviceGroup = groups.contains("idevice");

    return isInIdeviceGroup;
}

bool DiagnoseWidget::checkAvahiDaemonRunning()
{
    QProcess checkProcess;
    checkProcess.start("systemctl", QStringList()
                                        << "is-active" << "avahi-daemon");
    checkProcess.waitForFinished(3000);

    if (checkProcess.exitCode() != 0) {
        return false;
    }

    QString output =
        QString::fromUtf8(checkProcess.readAllStandardOutput()).trimmed();
    return output == "active";
}
#endif

void DiagnoseWidget::onToggleExpand()
{
    m_isExpanded = !m_isExpanded;
    m_itemsWidget->setVisible(m_isExpanded);
    m_toggleButton->setText(m_isExpanded ? "▲" : "▼");
    m_itemsWidget->updateGeometry();
    adjustSize();
}

#ifdef WIN32
void DiagnoseWidget::installBonjourRuntime()
{
    DependencyItem *itemToInstall = nullptr;
    for (DependencyItem *item : m_dependencyItems) {
        if (item->property("name").toString() == "Bonjour Service") {
            itemToInstall = item;
            break;
        }
    }

    if (!itemToInstall)
        return;

    itemToInstall->setInstalling(true);
    itemToInstall->setProgress("Downloading...");

    // Download Bonjour SDK
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(
        QUrl("https://github.com/tempx-x/bonjour-sdk/raw/refs/heads/main/"
             "bonjoursdksetup.exe"));

    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [itemToInstall](qint64 bytesReceived, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    int percent = (bytesReceived * 100) / bytesTotal;
                    itemToInstall->setProgress(
                        QString("Downloading... %1%").arg(percent));
                }
            });

    connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, manager, itemToInstall]() {
            reply->deleteLater();
            manager->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::critical(this, "Download Failed",
                                      "Failed to download Bonjour SDK: " +
                                          reply->errorString());
                checkDependencies(false);
                return;
            }

            itemToInstall->setProgress("Verifying...");

            // Verify MD5 checksum
            QByteArray data = reply->readAll();
            QByteArray hash =
                QCryptographicHash::hash(data, QCryptographicHash::Md5);
            QString actualHash = hash.toHex();
            QString expectedHash = "4ff2aae8205aec31b06743782cfcadce";

            if (actualHash != expectedHash) {
                QMessageBox::critical(
                    this, "Checksum Mismatch",
                    QString("Downloaded file checksum does not match!\n"
                            "Expected: %1\n"
                            "Got: %2")
                        .arg(expectedHash, actualHash));
                checkDependencies(false);
                return;
            }

            itemToInstall->setProgress("Extracting...");

            // Create temp directory
            QString tempDir =
                QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                "/bonjour_install";
            QDir().mkpath(tempDir);

            // Save the downloaded file
            QString exePath = tempDir + "/bonjoursdksetup.exe";
            QFile file(exePath);
            if (!file.open(QIODevice::WriteOnly)) {
                QMessageBox::critical(this, "Error",
                                      "Failed to save downloaded file");
                checkDependencies(false);
                return;
            }
            file.write(data);
            file.close();

            // Extract using libarchive
            struct archive *a = archive_read_new();
            archive_read_support_format_all(a);
            archive_read_support_filter_all(a);

            struct archive *ext = archive_write_disk_new();
            archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);

            if (archive_read_open_filename(a, exePath.toUtf8().constData(),
                                           10240) != ARCHIVE_OK) {
                QMessageBox::critical(this, "Extraction Failed",
                                      QString("Failed to open archive: %1")
                                          .arg(archive_error_string(a)));
                archive_read_free(a);
                archive_write_free(ext);
                checkDependencies(false);
                return;
            }

            struct archive_entry *entry;
            QString msiPath;

            while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                QString entryName =
                    QString::fromUtf8(archive_entry_pathname(entry));

                if (entryName.endsWith("Bonjour64.msi", Qt::CaseInsensitive)) {
                    QString fullPath = tempDir + "/" + entryName;
                    archive_entry_set_pathname(entry,
                                               fullPath.toUtf8().constData());

                    if (archive_write_header(ext, entry) != ARCHIVE_OK) {
                        qWarning() << "Failed to write header for" << entryName;
                    } else {
                        const void *buff;
                        size_t size;
                        la_int64_t offset;

                        while (archive_read_data_block(a, &buff, &size,
                                                       &offset) == ARCHIVE_OK) {
                            archive_write_data_block(ext, buff, size, offset);
                        }
                    }
                    archive_write_finish_entry(ext);
                    msiPath = fullPath;
                    break; // Only need Bonjour64.msi
                } else {
                    archive_read_data_skip(a);
                }
            }

            archive_read_free(a);
            archive_write_free(ext);

            if (msiPath.isEmpty()) {
                QMessageBox::critical(this, "Extraction Failed",
                                      "Could not find Bonjour64.msi in the "
                                      "archive");
                QDir(tempDir).removeRecursively();
                checkDependencies(false);
                return;
            }

            itemToInstall->setProgress("Installing...");

            // Launch the MSI via the shell (same behavior as double-click)
            itemToInstall->setInstalling(false); // we can't track MSI process

            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(msiPath))) {
                QMessageBox::warning(this, "Installation Failed",
                                     "Failed to launch Bonjour installer.\n\n"
                                     "You can also run it manually from:\n" +
                                         msiPath);
                checkDependencies(false);
                return;
            }

            QMessageBox::information(
                this, "Installation Started",
                "The Bonjour installer has been launched.\n"
                "Please complete the setup, then re-run the dependency check.");

            itemToInstall->setProgress("Refresh to verify installation.");
        });
}
#endif
