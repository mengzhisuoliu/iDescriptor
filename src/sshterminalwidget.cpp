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

#include "sshterminalwidget.h"
#include "qprocessindicator.h"
#include "settingsmanager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <libssh/libssh.h>
#include <qtermwidget6/qtermwidget.h>
#include <unistd.h>

SSHTerminalWidget::SSHTerminalWidget(const ConnectionInfo &connectionInfo,
                                     QWidget *parent)
    : QWidget(parent), m_connectionInfo(connectionInfo), m_sshSession(nullptr),
      m_sshChannel(nullptr), m_iproxyProcess(nullptr), m_sshConnected(false),
      m_isInitialized(false), m_currentState(TerminalState::Loading)
{
    setWindowTitle(QString("SSH Terminal / %1 - iDescriptor")
                       .arg(m_connectionInfo.deviceName));
    setMinimumSize(800, 600);

    setupUI();

    // Initialize SSH
    ssh_init();

    // Setup timer for checking SSH data
    m_sshTimer = new QTimer(this);
    connect(m_sshTimer, &QTimer::timeout, this,
            &SSHTerminalWidget::checkSshData);

    // Start connection process
    initializeConnection();
}

SSHTerminalWidget::~SSHTerminalWidget() { cleanup(); }

void SSHTerminalWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget);

    setupLoadingState();
    setupErrorState();
    setupActionState();

    setState(TerminalState::Loading);
}

void SSHTerminalWidget::setupLoadingState()
{
    m_loadingWidget = new QWidget();
    QVBoxLayout *loadingLayout = new QVBoxLayout(m_loadingWidget);
    loadingLayout->setAlignment(Qt::AlignCenter);

    // Process indicator
    m_loadingIndicator = new QProcessIndicator(m_loadingWidget);
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(64, 64);

    // Loading label
    m_loadingLabel = new QLabel("Connecting to SSH server...");
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet(
        "QLabel { font-size: 14px; color: #666; margin-top: 20px; }");

    loadingLayout->addWidget(m_loadingIndicator, 0, Qt::AlignCenter);
    loadingLayout->addWidget(m_loadingLabel);

    m_stackedWidget->addWidget(m_loadingWidget);
}

void SSHTerminalWidget::setupErrorState()
{
    m_errorWidget = new QWidget();
    QVBoxLayout *errorLayout = new QVBoxLayout(m_errorWidget);
    errorLayout->setAlignment(Qt::AlignCenter);
    errorLayout->setSpacing(20);

    // Error label
    m_errorLabel = new QLabel();
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(
        "QLabel { font-size: 14px; color: #d32f2f; padding: 20px; }");

    // Retry button
    m_retryButton = new QPushButton("Retry Connection");
    m_retryButton->setStyleSheet(
        "QPushButton { padding: 10px 20px; font-size: 14px; }");
    connect(m_retryButton, &QPushButton::clicked, this,
            &SSHTerminalWidget::onRetryClicked);

    errorLayout->addWidget(m_errorLabel);
    errorLayout->addWidget(m_retryButton, 0, Qt::AlignCenter);

    m_stackedWidget->addWidget(m_errorWidget);
}

void SSHTerminalWidget::setupActionState()
{
    m_actionWidget = new QWidget();
    QVBoxLayout *actionLayout = new QVBoxLayout(m_actionWidget);
    actionLayout->setContentsMargins(0, 0, 0, 0);

    // Terminal widget
    m_terminal = new QTermWidget(0, m_actionWidget);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->setColorScheme("Linux");
    m_terminal->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_terminal, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                QMenu menu(this);
                QList<QAction *> actions = m_terminal->filterActions(pos);
                if (!actions.isEmpty()) {
                    menu.addActions(actions);
                    menu.exec(m_terminal->mapToGlobal(pos));
                }
            });
#ifdef WIN32
    m_terminal->startTerminalEmulation();
#else
    m_terminal->startTerminalTeletype();
#endif
    m_terminal->setStyleSheet("padding: 5px;");

    actionLayout->addWidget(m_terminal);

    m_stackedWidget->addWidget(m_actionWidget);
}

void SSHTerminalWidget::setState(TerminalState state)
{
    m_currentState = state;

    switch (state) {
    case TerminalState::Loading:
        m_stackedWidget->setCurrentWidget(m_loadingWidget);
        m_loadingIndicator->start();
        break;

    case TerminalState::Error:
        m_stackedWidget->setCurrentWidget(m_errorWidget);
        m_loadingIndicator->stop();
        break;

    case TerminalState::Connected:
        m_stackedWidget->setCurrentWidget(m_actionWidget);
        m_loadingIndicator->stop();
        m_terminal->setFocus();
        break;
    }
}

void SSHTerminalWidget::showError(const QString &errorMessage)
{
    m_errorLabel->setText(errorMessage);
    setState(TerminalState::Error);
}

void SSHTerminalWidget::onRetryClicked()
{
    // Reset all state
    cleanup();
    m_sshConnected = false;
    m_isInitialized = false;

    // Reinitialize SSH
    ssh_init();

    // Setup timer again
    m_sshTimer = new QTimer(this);
    connect(m_sshTimer, &QTimer::timeout, this,
            &SSHTerminalWidget::checkSshData);

    // Update loading message and start connection
    m_loadingLabel->setText("Connecting to SSH server...");
    setState(TerminalState::Loading);
    initializeConnection();
}

void SSHTerminalWidget::initializeConnection()
{
    if (m_connectionInfo.type == ConnectionType::Wired) {
        initWiredDevice();
    } else {
        initWirelessDevice();
    }
}

void SSHTerminalWidget::initWiredDevice()
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;

    m_loadingLabel->setText("Setting up SSH tunnel...");

    // Start iproxy for wired devices
    m_iproxyProcess = new QProcess(this);
    m_iproxyProcess->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", env.value("PATH") + ":/usr/local/bin:/opt/homebrew/bin");
    m_iproxyProcess->setProcessEnvironment(env);

    connect(m_iproxyProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                // showError("Error starting iproxy: " +
                // m_iproxyProcess->errorString());
                qDebug() << "iproxy error:" << error;
            });

    connect(m_iproxyProcess, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                qDebug() << "iproxy finished with exit code:" << exitCode;
                if (!m_sshConnected) {
                    // showError("iproxy process terminated unexpectedly");
                }
            });

    // Monitor iproxy output for readiness
    connect(m_iproxyProcess, &QProcess::readyRead, this, [this]() {
        QByteArray output = m_iproxyProcess->readAll();
        qDebug() << "iproxy output:" << output;

        if (output.contains("waiting for connection")) {
            qDebug() << "iproxy is ready, starting SSH connection";
            disconnect(m_iproxyProcess, &QProcess::readyRead, this, nullptr);
            startSSH(QHostAddress(QHostAddress::LocalHost).toString(), 3333);
        } else if (output.contains("ERROR") || output.contains("failed")) {
            qDebug() << "iproxy error detected in output" << output;
            // showError("iproxy failed: " + QString::fromUtf8(output));
        }
    });

    // Add timeout timer as backup
    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this, timeoutTimer]() {
        qDebug() << "iproxy timeout - assuming it's ready and attempting SSH "
                    "connection";
        timeoutTimer->deleteLater();
        startSSH(QHostAddress(QHostAddress::LocalHost).toString(), 3333);
    });

    QStringList args;
    args << "-u" << m_connectionInfo.deviceUdid << "3333" << "22";

    qDebug() << "Starting iproxy with args:" << args;

    QString iproxyPath;
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString bundledIproxyPath = appDirPath + "/iproxy";

    /* MacOS bundled iproxy */
    if (QFileInfo(bundledIproxyPath).isExecutable()) {
        iproxyPath = bundledIproxyPath;
    }
    /* AppImage - this is set by the plugin script */
    else if (qEnvironmentVariableIsSet("IPROXY_BIN_APPIMAGE")) {
        iproxyPath = qgetenv("IPROXY_BIN_APPIMAGE");
        if (iproxyPath.isEmpty()) {
            showError("Error: Running in AppImage mode, but "
                      "IPROXY_BIN_APPIMAGE is not set.");
            return;
        }

        if (!QFileInfo(iproxyPath).isExecutable()) {
            showError(
                "Error: Bundled iproxy not found or is not executable at " +
                iproxyPath);
            return;
        }
    } else {
        iproxyPath = QStandardPaths::findExecutable("iproxy");
        if (iproxyPath.isEmpty()) {
            showError(
                "Error: iproxy not found. Please install libimobiledevice.");
            return;
        }
    }

    qDebug() << "Using iproxy at:" << iproxyPath;
    m_iproxyProcess->start(iproxyPath, args);

    if (!m_iproxyProcess->waitForStarted(5000)) {
        showError("Failed to start iproxy process");
        timeoutTimer->deleteLater();
        return;
    }

    qDebug() << "iproxy process started, waiting for readiness...";
    timeoutTimer->start(5000);
}

void SSHTerminalWidget::initWirelessDevice()
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;

    m_loadingLabel->setText("Connecting to network device...");

    // For wireless devices, connect directly without iproxy
    startSSH(m_connectionInfo.hostAddress, m_connectionInfo.port);
}

void SSHTerminalWidget::startSSH(const QString &host, uint16_t port)
{
    qDebug() << "Starting SSH to" << host << "on port" << port;

    QString defaultPassword =
        SettingsManager::sharedInstance()->defaultJailbrokenRootPassword();
    QByteArray passwordBytes = defaultPassword.toUtf8();

    bool ok;
    QString code =
        QInputDialog::getText(nullptr, "SSH Root Password",
                              "Enter the root password: \n(leave empty if you "
                              "want to use the default)",
                              QLineEdit::Normal, QString(), &ok);

    if (!ok) {
        showError("Root password input canceled");
        return;
    }

    if (m_sshConnected)
        return;

    m_loadingLabel->setText("Establishing SSH connection...");
    qDebug() << "Starting SSH connection to" << host << ":" << port;

    // Create SSH session
    m_sshSession = ssh_new();
    if (!m_sshSession) {
        showError("Error: Failed to create SSH session");
        return;
    }

    // Configure SSH session
    QByteArray hostBytes = host.toUtf8();
    ssh_options_set(m_sshSession, SSH_OPTIONS_HOST, hostBytes.constData());
    int sshPort = static_cast<int>(port);
    ssh_options_set(m_sshSession, SSH_OPTIONS_PORT, &sshPort);
    ssh_options_set(m_sshSession, SSH_OPTIONS_USER, "root");

    // Disable strict host key checking
    int stricthostcheck = 0;
    ssh_options_set(m_sshSession, SSH_OPTIONS_STRICTHOSTKEYCHECK,
                    &stricthostcheck);

    // Set log level for debugging
    int log_level = SSH_LOG_PROTOCOL;
    ssh_options_set(m_sshSession, SSH_OPTIONS_LOG_VERBOSITY, &log_level);

    qDebug() << "SSH session configured, attempting connection...";

    // Connect to SSH server
    int rc = ssh_connect(m_sshSession);
    qDebug() << "SSH connect result:" << rc << "SSH_OK:" << SSH_OK;
    if (rc != SSH_OK) {
        QString errorMsg = QString("SSH connection failed: %1")
                               .arg(ssh_get_error(m_sshSession));
        showError(errorMsg);
        qDebug() << errorMsg;
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return;
    }

    qDebug() << "SSH connected successfully, attempting authentication...";

    rc =
        ssh_userauth_password(m_sshSession, nullptr, passwordBytes.constData());
    if (rc != SSH_AUTH_SUCCESS) {
        showError(QString("SSH authentication failed: %1")
                      .arg(ssh_get_error(m_sshSession)));
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return;
    }

    // Create SSH channel
    m_sshChannel = ssh_channel_new(m_sshSession);
    if (!m_sshChannel) {
        showError("Error: Failed to create SSH channel");
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return;
    }

    // Open SSH channel
    rc = ssh_channel_open_session(m_sshChannel);
    if (rc != SSH_OK) {
        showError(QString("Failed to open SSH channel: %1")
                      .arg(ssh_get_error(m_sshSession)));
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return;
    }

    // Request a PTY
    rc = ssh_channel_request_pty(m_sshChannel);
    if (rc != SSH_OK) {
        showError("Failed to request PTY");
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return;
    }

    // Start shell
    rc = ssh_channel_request_shell(m_sshChannel);
    if (rc != SSH_OK) {
        showError("Failed to start shell");
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        return;
    }

    // Connect terminal to SSH
    connectLibsshToTerminal();

    // Start timer to check for SSH data
    m_sshTimer->start(50); // Check every 50ms

    m_sshConnected = true;
    setState(TerminalState::Connected);

    qDebug() << "SSH terminal connected successfully";
}

void SSHTerminalWidget::connectLibsshToTerminal()
{
    if (!m_terminal)
        return;

    // Connect terminal input to SSH channel
    connect(m_terminal, &QTermWidget::sendData, this,
            [this](const char *data, int size) {
                if (m_sshChannel && ssh_channel_is_open(m_sshChannel)) {
                    ssh_channel_write(m_sshChannel, data, size);
                }
            });
}

void SSHTerminalWidget::checkSshData()
{
    if (!m_sshChannel || !ssh_channel_is_open(m_sshChannel))
        return;

    // Check if SSH channel has data to read
    if (ssh_channel_poll(m_sshChannel, 0) > 0) {
        char buffer[4096];
        int nbytes = ssh_channel_read_nonblocking(m_sshChannel, buffer,
                                                  sizeof(buffer), 0);
        if (nbytes > 0) {
#ifdef WIN32
            m_terminal->receiveData(buffer, nbytes);
#else
            // Write data to terminal's PTY
            write(m_terminal->getPtySlaveFd(), buffer, nbytes);
#endif
        }
    }

    // Check for stderr data
    if (ssh_channel_poll(m_sshChannel, 1) > 0) {
        char buffer[4096];
        int nbytes = ssh_channel_read_nonblocking(m_sshChannel, buffer,
                                                  sizeof(buffer), 1);
        if (nbytes > 0) {
            // Write stderr data to terminal's PTY
#ifdef WIN32
            m_terminal->receiveData(buffer, nbytes);
#else
            write(m_terminal->getPtySlaveFd(), buffer, nbytes);
#endif
        }
    }

    // Check if channel is closed
    if (ssh_channel_is_eof(m_sshChannel)) {
        disconnectSSH();
    }
}

void SSHTerminalWidget::disconnectSSH()
{
    cleanup();
    showError("SSH connection was closed by the remote host");
}

void SSHTerminalWidget::cleanup()
{
    if (m_sshTimer) {
        m_sshTimer->stop();
        m_sshTimer->deleteLater();
        m_sshTimer = nullptr;
    }

    if (m_sshChannel) {
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
    }

    if (m_sshSession) {
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
    }

    if (m_iproxyProcess) {
        // Properly terminate iproxy process
        if (m_iproxyProcess->state() != QProcess::NotRunning) {
            m_iproxyProcess->terminate(); // Send SIGTERM first
            if (!m_iproxyProcess->waitForFinished(3000)) {
                qDebug() << "iproxy didn't terminate gracefully, killing...";
                m_iproxyProcess->kill(); // Force kill if needed
                m_iproxyProcess->waitForFinished(1000);
            }
        }
        m_iproxyProcess->deleteLater();
        m_iproxyProcess = nullptr;
    }

    m_sshConnected = false;
}
