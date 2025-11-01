#include "ifusewidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "ifusediskunmountbutton.h"
#include "ifusemanager.h"
#include "mainwindow.h"
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>

iFuseWidget::iFuseWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_mainLayout(nullptr), m_ifuseProcess(nullptr),
      m_device(device)
{
    setupUI();
    updateUI();

    connect(AppContext::sharedInstance(), &AppContext::deviceChange, this,
            &iFuseWidget::updateUI);
}

void iFuseWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Description label
    m_descriptionLabel = new QLabel("This tool allows you to mount your "
                                    "iPhone's disk as a drive on your PC");
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setStyleSheet(
        "font-size: 14px; color: #666; margin-bottom: 10px;");
    m_mainLayout->addWidget(m_descriptionLabel);

    // Status label
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();
    m_statusLabel->setStyleSheet(
        "padding: 8px; border-radius: 4px; margin: 5px 0;");
    m_mainLayout->addWidget(m_statusLabel);

    // Device selection
    QWidget *deviceWidget = new QWidget();
    QHBoxLayout *deviceLayout = new QHBoxLayout(deviceWidget);
    deviceLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *deviceLabel = new QLabel("Select Device:");
    deviceLabel->setMinimumWidth(100);
    m_deviceComboBox = new QComboBox();
    m_deviceComboBox->setMinimumHeight(35);

    deviceLayout->addWidget(deviceLabel);
    deviceLayout->addWidget(m_deviceComboBox, 1);
    m_mainLayout->addWidget(deviceWidget);

    // Mount path selection
    QWidget *pathWidget = new QWidget();
    QHBoxLayout *pathLayout = new QHBoxLayout(pathWidget);
    pathLayout->setContentsMargins(0, 0, 0, 0);

    m_mountPathLabel = new ZLabel(this);
    m_mountPathLabel->setCursor(Qt::PointingHandCursor);
    m_mountPathLabel->setText("Mount directory will be shown here");
    m_mountPathLabel->setStyleSheet("QLabel { "
                                    "border: 1px solid #ccc; "
                                    "padding: 8px; "
                                    "border-radius: 4px; "
                                    "}");
    m_mountPathLabel->setMinimumHeight(35);

    m_folderPickerButton = new QPushButton("Browse...");
    m_folderPickerButton->setMinimumHeight(35);

    pathLayout->addWidget(m_mountPathLabel, 1);
    pathLayout->addWidget(m_folderPickerButton);
    m_mainLayout->addWidget(pathWidget);

    // Mount button
    m_mountButton = new QPushButton("Mount Device");
    m_mountButton->setMinimumHeight(40);
    m_mountButton->setDefault(true);
    m_mainLayout->addWidget(m_mountButton);

    // Add stretch to push everything to the top
    m_mainLayout->addStretch();

    // Connect signals
    connect(m_folderPickerButton, &QPushButton::clicked, this,
            &iFuseWidget::onFolderPickerClicked);
    connect(m_mountPathLabel, &ZLabel::clicked, this,
            &iFuseWidget::onMountPathClicked);
    connect(m_mountButton, &QPushButton::clicked, this,
            &iFuseWidget::onMountClicked);

    connect(m_deviceComboBox, &QComboBox::currentTextChanged, this,
            &iFuseWidget::onDeviceChanged);

    QString homeDir =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString productType =
        QString::fromStdString(m_device->deviceInfo.productType);
    QString defaultMountPath = QDir(homeDir).absoluteFilePath(productType);
    m_mountPathLabel->setText(defaultMountPath);
}

void iFuseWidget::updateDeviceComboBox()
{
    QList<iDescriptorDevice *> devices =
        AppContext::sharedInstance()->getAllDevices();

    m_deviceComboBox->blockSignals(true);
    m_deviceComboBox->clear();
    m_deviceComboBox->setEnabled(true);
    m_mountButton->setEnabled(true);

    for (iDescriptorDevice *device : devices) {
        QString displayText =
            QString::fromStdString(device->deviceInfo.productType) + " / " +
            QString::fromStdString(device->udid);
        m_deviceComboBox->addItem(displayText,
                                  QString::fromStdString(device->udid));
    }
    m_deviceComboBox->blockSignals(false);

    // Try to find and select the device passed to the widget
    int deviceIndex = -1;
    if (m_device) {
        deviceIndex =
            m_deviceComboBox->findData(QString::fromStdString(m_device->udid));
    }

    if (deviceIndex != -1) {
        // Found the pre-selected device, so select it.
        m_deviceComboBox->setCurrentIndex(deviceIndex);
    } else if (!devices.isEmpty()) {
        // Pre-selected device not found or not provided, so select the first
        // one.
        m_device = devices.first();
        m_deviceComboBox->setCurrentIndex(0);
    }
}

void iFuseWidget::onFolderPickerClicked()
{
#ifdef WIN32
    QString currentPath = m_mountPathLabel->text();
    // On Windows, ifuse requires a non-existent directory.
    // We can use getSaveFileName to allow the user to specify one.
    QString dir = QFileDialog::getSaveFileName(this, "Select Mount Directory",
                                               currentPath);
    if (!dir.isEmpty()) {
        m_mountPathLabel->setText(dir);
    }
#endif

#ifdef __linux__
    QString currentPath = m_mountPathLabel->text();
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Mount Directory", currentPath);
    if (!dir.isEmpty()) {
        m_mountPathLabel->setText(dir);
    }
#endif
}

void iFuseWidget::onMountPathClicked()
{
    QString currentPath = m_mountPathLabel->text();
    if (!currentPath.isEmpty() && QDir(currentPath).exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(currentPath));
    }
}

void iFuseWidget::onMountClicked()
{
    if (!validateInputs()) {
        return;
    }

    m_ifuseProcess = new QProcess();
    connect(m_ifuseProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &iFuseWidget::onProcessFinished);
    connect(m_ifuseProcess, &QProcess::errorOccurred, this,
            &iFuseWidget::onProcessError);

    QString ifuseExecutablePath;
    QString fullMountPath = m_mountPathLabel->text();

// On Windows we ship with a bundled win-ifuse.exe
#ifdef WIN32
    ifuseExecutablePath =
        QCoreApplication::applicationDirPath() + "/win-ifuse.exe";
    qDebug() << "Looking for bundled win-ifuse.exe at" << ifuseExecutablePath;
    if (!QFileInfo::exists(ifuseExecutablePath)) {
        setStatusMessage("Error: win-ifuse.exe not found at expected path: " +
                             ifuseExecutablePath,
                         true);
        return;
    }
#endif

#ifdef __linux__
    /*
        Check if running in AppImage
        this is set by the plugin script
    */
    if (qEnvironmentVariableIsSet("IFUSE_BIN_APPIMAGE")) {
        ifuseExecutablePath = qgetenv("IFUSE_BIN_APPIMAGE");
        if (ifuseExecutablePath.isEmpty()) {
            setStatusMessage("Error: Running in AppImage mode, but "
                             "IFUSE_BIN_APPIMAGE is not set.",
                             true);
            return;
        }

        if (!QFileInfo(ifuseExecutablePath).isExecutable()) {
            setStatusMessage("Error: ifuse not found or is not executable.",
                             true);
            return;
        }
    } else {
        ifuseExecutablePath = QStandardPaths::findExecutable("ifuse");
        if (ifuseExecutablePath.isEmpty()) {
            setStatusMessage(
                "Error: ifuse binary not found. Please install ifuse first.",
                true);
            return;
        }
    }
#endif

#ifdef WIN32
    // On Windows, the mount path must not exist.
    if (QFileInfo(fullMountPath).exists()) {
        setStatusMessage("Error: Mount directory must not exist on Windows: " +
                             fullMountPath,
                         true);
        return;
    }
#endif

    QDir dir;
// on Linux, we need to create the mount directory if it doesn't exist
#ifdef __linux__
    if (!QDir(fullMountPath).exists()) {
        if (!dir.mkpath(fullMountPath)) {
            setStatusMessage("Error: Failed to create mount directory: " +
                                 fullMountPath,
                             true);
            return;
        }
    }
#endif

    m_currentMountPath = fullMountPath;

    QString deviceUdid = getSelectedDeviceUdid();

    setStatusMessage("Mounting device...", false);
    m_mountButton->setText("Mounting...");
    m_mountButton->setEnabled(false);

    // Run ifuse command
    QStringList arguments;
    arguments << "-u" << deviceUdid << fullMountPath;

    m_ifuseProcess->start(ifuseExecutablePath, arguments);

#ifdef WIN32
    // On Windows, the process runs in the foreground. We wait for it to start.
    // If it fails to start, onProcessError will be called.
    if (!m_ifuseProcess->waitForStarted()) {
        return;
    }

    // Process started successfully, so we treat it as a successful mount
    setStatusMessage("Device mounted successfully at: " + m_currentMountPath,
                     false);

    m_mountButton->setText("Mount");
    m_mountButton->setEnabled(true);

    auto *b = new iFuseDiskUnmountButton(m_currentMountPath);
    MainWindow::sharedInstance()->statusBar()->addPermanentWidget(b);

    QProcess *processToKill = m_ifuseProcess;
    connect(b, &iFuseDiskUnmountButton::clicked, b, [processToKill, b]() {
        if (processToKill) {
            processToKill->kill();
        }
        MainWindow::sharedInstance()->statusBar()->removeWidget(b);
        b->deleteLater();
    });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, b,
            [processToKill]() {
                if (processToKill &&
                    processToKill->state() == QProcess::Running) {
                    processToKill->kill();
                }
            });

    QTimer::singleShot(1000, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentMountPath));
    });
#endif
}

void iFuseWidget::onProcessFinished(int exitCode,
                                    QProcess::ExitStatus exitStatus)
{
    m_mountButton->setText("Mount Device");
    m_mountButton->setEnabled(true);

    if (exitStatus == QProcess::CrashExit) {
        setStatusMessage("Error: ifuse process crashed", true);
        return;
    }

#ifdef WIN32
    // On Windows, this is just a confirmation that the process has been
    // terminated.
    setStatusMessage("Device unmounted from " + m_currentMountPath, false);
#endif

#ifdef __linux__
    if (exitCode == 0) {
        setStatusMessage(
            "Device mounted successfully at: " + m_currentMountPath, false);

        auto *b = new iFuseDiskUnmountButton(m_currentMountPath);
        MainWindow::sharedInstance()->statusBar()->addPermanentWidget(b);
        QProcess *processToKill = m_ifuseProcess;
        QString currentMountPath = m_currentMountPath;
        connect(b, &iFuseDiskUnmountButton::clicked, this,
                [b, processToKill, currentMountPath]() {
                    qDebug() << "Unmounting" << currentMountPath;
                    bool ok = iFuseManager::linuxUnmount(currentMountPath);
                    if (!ok) {
                        QMessageBox::warning(nullptr, "Unmount Failed",
                                             "Failed to unmount iFuse at " +
                                                 currentMountPath +
                                                 ". Please try again.");
                        return;
                    }
                    MainWindow::sharedInstance()->statusBar()->removeWidget(b);
                    b->deleteLater();
                });
        QDesktopServices::openUrl(QUrl::fromLocalFile(currentMountPath));
    } else {
        QString errorOutput = m_ifuseProcess->readAllStandardError();
        setStatusMessage("Mount failed: " + errorOutput, true);
    }
#endif

    m_ifuseProcess->deleteLater();
    m_ifuseProcess = nullptr;
}

void iFuseWidget::onProcessError(QProcess::ProcessError error)
{
    m_mountButton->setText("Mount Device");
    m_mountButton->setEnabled(true);

    QString errorMessage;
    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = "Failed to start ifuse. Make sure it's installed.";
        break;
    case QProcess::Crashed:
        errorMessage = "ifuse process crashed.";
        break;
    case QProcess::Timedout:
        errorMessage = "ifuse process timed out.";
        break;
    default:
        errorMessage = "Unknown error occurred.";
        break;
    }

    setStatusMessage("Error: " + errorMessage, true);

    if (m_ifuseProcess) {
        m_ifuseProcess->deleteLater();
        m_ifuseProcess = nullptr;
    }
}

void iFuseWidget::updatePath()
{
    QString homeDir =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString productType =
        QString::fromStdString(m_device->deviceInfo.productType);
    QString defaultMountPath = QDir(homeDir).absoluteFilePath(productType);
    m_mountPathLabel->setText(defaultMountPath);
}

void iFuseWidget::updateUI()
{
    QList<iDescriptorDevice *> devices =
        AppContext::sharedInstance()->getAllDevices();

    if (devices.isEmpty()) {
        m_device = nullptr;
        m_deviceComboBox->clear();
        m_deviceComboBox->setEnabled(false);
        m_mountButton->setEnabled(false);
        m_mountPathLabel->setText("No device connected.");
        deleteLater();
        return;
    }
    updateDeviceComboBox();
    updatePath();
}

bool iFuseWidget::validateInputs()
{
    if (m_deviceComboBox->currentData().toString().isEmpty()) {
        setStatusMessage("Error: No device selected", true);
        return false;
    }

    return true;
}

QString iFuseWidget::getSelectedDeviceUdid()
{
    return m_deviceComboBox->currentData().toString();
}

void iFuseWidget::setStatusMessage(const QString &message, bool isError)
{
    m_statusLabel->setText(message);
    m_statusLabel->show();

    if (isError) {
        m_statusLabel->setStyleSheet(
            "background-color: #ffe6e6; color: #d00; border: 1px solid "
            "#ffcccc; padding: 8px; border-radius: 4px; margin: 5px 0;");
    } else {
        m_statusLabel->setStyleSheet(
            "background-color: #e6ffe6; color: #060; border: 1px solid "
            "#ccffcc; padding: 8px; border-radius: 4px; margin: 5px 0;");
    }

    // Auto-hide status after 5 seconds for non-error messages
    if (!isError) {
        QTimer::singleShot(5000, [this]() { m_statusLabel->hide(); });
    }
}

void iFuseWidget::onDeviceChanged(const QString &text)
{
    QString selectedUdid = m_deviceComboBox->currentData().toString();
    QList<iDescriptorDevice *> devices =
        AppContext::sharedInstance()->getAllDevices();

    for (iDescriptorDevice *device : devices) {
        if (QString::fromStdString(device->udid) == selectedUdid) {
            m_device = device;

            // Update mount path to reflect new device
            QString homeDir =
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
            QString productType =
                QString::fromStdString(device->deviceInfo.productType);
            QString newMountPath = QDir(homeDir).absoluteFilePath(productType);
            m_mountPathLabel->setText(newMountPath);

            break;
        }
    }
}