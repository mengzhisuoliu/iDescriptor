#include "jailbrokenwidget.h"
#include "appcontext.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <libssh/libssh.h>
#include <qtermwidget6/qtermwidget.h>
#include <unistd.h>

JailbrokenWidget::JailbrokenWidget(QWidget *parent) : QWidget{parent}
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    QGraphicsScene *scene = new QGraphicsScene(this);
    QGraphicsPixmapItem *pixmapItem =
        new QGraphicsPixmapItem(QPixmap(":/resources/iphone.png"));
    scene->addItem(pixmapItem);

    QGraphicsView *graphicsView = new ResponsiveGraphicsView(scene, this);
    graphicsView->setRenderHint(QPainter::Antialiasing);
    graphicsView->setMinimumWidth(200);
    graphicsView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    graphicsView->setStyleSheet("background:transparent; border: none;");

    mainLayout->addWidget(graphicsView, 1);

    connect(AppContext::sharedInstance(), &AppContext::deviceAdded, this,
            [this](iDescriptorDevice *device) { deviceConnected(device); });

    // Right side: Info and Terminal
    QWidget *rightContainer = new QWidget();
    rightContainer->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
    rightContainer->setMinimumWidth(400);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(15, 15, 15, 15);
    rightLayout->setSpacing(10);

    m_infoLabel = new QLabel("Connect a jailbroken device");
    rightLayout->addWidget(m_infoLabel);

    m_connectButton = new QPushButton("Connect SSH Terminal");
    m_connectButton->setEnabled(false);
    connect(m_connectButton, &QPushButton::clicked, this,
            &JailbrokenWidget::onConnectSSH);
    rightLayout->addWidget(m_connectButton);

    setupTerminal();
    rightLayout->addWidget(m_terminal, 1);

    mainLayout->addWidget(rightContainer, 3);

    // Initialize SSH
    ssh_init();
    m_sshSession = nullptr;
    m_sshChannel = nullptr;

    // Setup timer for checking SSH data
    m_sshTimer = new QTimer(this);
    connect(m_sshTimer, &QTimer::timeout, this,
            &JailbrokenWidget::checkSshData);
}

void JailbrokenWidget::setupTerminal()
{
    m_terminal = new QTermWidget(0, this);
    m_terminal->setMinimumHeight(400);
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->setColorScheme("DarkPastels");
    m_terminal->startTerminalTeletype();
    m_terminal->hide();
}

void JailbrokenWidget::connectLibsshToTerminal()
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

void JailbrokenWidget::deviceConnected(iDescriptorDevice *device)
{
    if (device->deviceInfo.jailbroken) {
        m_device = device;
        m_infoLabel->setText("Jailbroken device connected");
        m_connectButton->setEnabled(true);
        m_connectButton->setText("Connect SSH Terminal");
    }
}

void JailbrokenWidget::onConnectSSH()
{
    if (m_sshConnected) {
        disconnectSSH();
        return;
    }

    initWidget();
}

void JailbrokenWidget::initWidget()
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;

    if (!m_device) {
        m_infoLabel->setText("Device is not jailbroken");
        return;
    }

    m_connectButton->setEnabled(false);
    m_infoLabel->setText("Setting up SSH tunnel...");

    // Start iproxy first
    iproxyProcess = new QProcess(this);
    iproxyProcess->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Add common directories where iproxy might be installed
    env.insert("PATH", env.value("PATH") + ":/usr/local/bin:/opt/homebrew/bin");

    iproxyProcess->setProcessEnvironment(env);

    connect(iproxyProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                m_infoLabel->setText("Error: " + iproxyProcess->errorString());
                m_connectButton->setEnabled(true);
                qDebug() << "iproxy error:" << error;
            });

    QTimer::singleShot(
        3000, this,
        &JailbrokenWidget::startSSH); // Increased delay to 3 seconds

    iproxyProcess->start("iproxy", QStringList()
                                       << "-u" << m_device->udid.c_str()
                                       << "3333" << "22");

    // Check if iproxy started successfully
    if (!iproxyProcess->waitForStarted(5000)) {
        m_infoLabel->setText("Failed to start iproxy");
        m_connectButton->setEnabled(true);
        return;
    }
}

void JailbrokenWidget::startSSH()
{
    if (m_sshConnected)
        return;

    m_infoLabel->setText("Connecting to SSH server...");
    qDebug() << "Starting SSH connection to localhost:3333";

    // Create SSH session
    m_sshSession = ssh_new();
    if (!m_sshSession) {
        m_infoLabel->setText("Error: Failed to create SSH session");
        m_connectButton->setEnabled(true);
        return;
    }

    // Configure SSH session
    ssh_options_set(m_sshSession, SSH_OPTIONS_HOST, "localhost");
    int port = 3333;
    ssh_options_set(m_sshSession, SSH_OPTIONS_PORT, &port);
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
        m_infoLabel->setText(errorMsg);
        qDebug() << errorMsg;
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    qDebug() << "SSH connected successfully, attempting authentication...";

    // Authenticate with password
    rc = ssh_userauth_password(m_sshSession, nullptr, "alpine");
    if (rc != SSH_AUTH_SUCCESS) {
        m_infoLabel->setText(QString("SSH authentication failed: %1")
                                 .arg(ssh_get_error(m_sshSession)));
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Create SSH channel
    m_sshChannel = ssh_channel_new(m_sshSession);
    if (!m_sshChannel) {
        m_infoLabel->setText("Error: Failed to create SSH channel");
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Open SSH channel
    rc = ssh_channel_open_session(m_sshChannel);
    if (rc != SSH_OK) {
        m_infoLabel->setText(QString("Failed to open SSH channel: %1")
                                 .arg(ssh_get_error(m_sshSession)));
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Request a PTY
    rc = ssh_channel_request_pty(m_sshChannel);
    if (rc != SSH_OK) {
        m_infoLabel->setText("Failed to request PTY");
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Start shell
    rc = ssh_channel_request_shell(m_sshChannel);
    if (rc != SSH_OK) {
        m_infoLabel->setText("Failed to start shell");
        ssh_channel_close(m_sshChannel);
        ssh_channel_free(m_sshChannel);
        m_sshChannel = nullptr;
        ssh_disconnect(m_sshSession);
        ssh_free(m_sshSession);
        m_sshSession = nullptr;
        m_connectButton->setEnabled(true);
        return;
    }

    // Show terminal and connect to libssh
    m_terminal->show();
    connectLibsshToTerminal();

    // Start timer to check for SSH data
    m_sshTimer->start(50); // Check every 50ms

    m_sshConnected = true;
    m_connectButton->setEnabled(true);
    m_connectButton->setText("Disconnect SSH");
    m_infoLabel->setText("SSH terminal connected");

    // Set focus to terminal
    m_terminal->setFocus();
}

void JailbrokenWidget::checkSshData()
{
    if (!m_sshChannel || !ssh_channel_is_open(m_sshChannel))
        return;

    // Check if SSH channel has data to read
    if (ssh_channel_poll(m_sshChannel, 0) > 0) {
        char buffer[4096];
        int nbytes = ssh_channel_read_nonblocking(m_sshChannel, buffer,
                                                  sizeof(buffer), 0);
        if (nbytes > 0) {
            // Write data to terminal's PTY
            write(m_terminal->getPtySlaveFd(), buffer, nbytes);
        }
    }

    // Check for stderr data
    if (ssh_channel_poll(m_sshChannel, 1) > 0) {
        char buffer[4096];
        int nbytes = ssh_channel_read_nonblocking(m_sshChannel, buffer,
                                                  sizeof(buffer), 1);
        if (nbytes > 0) {
            // Write stderr data to terminal's PTY
            write(m_terminal->getPtySlaveFd(), buffer, nbytes);
        }
    }

    // Check if channel is closed
    if (ssh_channel_is_eof(m_sshChannel)) {
        disconnectSSH();
    }
}

void JailbrokenWidget::disconnectSSH()
{
    if (m_sshTimer) {
        m_sshTimer->stop();
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

    if (iproxyProcess) {
        iproxyProcess->terminate();
        iproxyProcess->waitForFinished(3000);
        iproxyProcess = nullptr;
    }

    m_terminal->hide();
    m_connectButton->setText("Connect SSH Terminal");
    m_infoLabel->setText("SSH disconnected");
    m_sshConnected = false;
    m_isInitialized = false;
    m_connectButton->setEnabled(true);
}

JailbrokenWidget::~JailbrokenWidget() { disconnectSSH(); }
