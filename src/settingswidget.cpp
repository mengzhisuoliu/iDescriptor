#include "settingswidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

SettingsWidget::SettingsWidget(QWidget *parent) : QDialog{parent}
{
    setupUI();
    loadSettings();
    connectSignals();
}

void SettingsWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(15);

    // Create scroll area for the settings
    auto *scrollArea = new QScrollArea();
    auto *scrollWidget = new QWidget();
    auto *scrollLayout = new QVBoxLayout(scrollWidget);

    // === GENERAL SETTINGS ===
    auto *generalGroup = new QGroupBox("General");
    auto *generalLayout = new QVBoxLayout(generalGroup);

    // Download path
    auto *downloadLayout = new QHBoxLayout();
    downloadLayout->addWidget(new QLabel("Download Path:"));
    m_downloadPathEdit = new QLineEdit();
    m_downloadPathEdit->setReadOnly(true);
    m_downloadPathEdit->setMaximumWidth(300);
    downloadLayout->addWidget(m_downloadPathEdit);
    auto *browseButton = new QPushButton("Browse...");
    downloadLayout->addWidget(browseButton);
    generalLayout->addLayout(downloadLayout);

    connect(browseButton, &QPushButton::clicked, this,
            &SettingsWidget::onBrowseButtonClicked);

    // Auto-check for updates
    m_autoUpdateCheck = new QCheckBox("Automatically check for updates");
    generalLayout->addWidget(m_autoUpdateCheck);

    // Theme selection
    auto *themeLayout = new QHBoxLayout();
    themeLayout->addWidget(new QLabel("Theme:"));
    m_themeCombo = new QComboBox();
    m_themeCombo->addItems({"System Default", "Light", "Dark"});
    themeLayout->addWidget(m_themeCombo);
    themeLayout->addStretch();
    generalLayout->addLayout(themeLayout);

    scrollLayout->addWidget(generalGroup);

    // === DEVICE CONNECTION SETTINGS ===
    auto *deviceGroup = new QGroupBox("Device Connection");
    auto *deviceLayout = new QVBoxLayout(deviceGroup);

    // Connection timeout
    auto *timeoutLayout = new QHBoxLayout();
    timeoutLayout->addWidget(new QLabel("Connection Timeout:"));
    m_connectionTimeout = new QSpinBox();
    m_connectionTimeout->setRange(5, 60);
    m_connectionTimeout->setSuffix(" seconds");
    timeoutLayout->addWidget(m_connectionTimeout);
    timeoutLayout->addStretch();
    deviceLayout->addLayout(timeoutLayout);

    // Auto-detect devices
    m_autoDetectDevices =
        new QCheckBox("Automatically detect connected devices");
    deviceLayout->addWidget(m_autoDetectDevices);

    // Show device notifications
    m_showDeviceNotifications =
        new QCheckBox("Show notifications when devices connect/disconnect");
    deviceLayout->addWidget(m_showDeviceNotifications);

    scrollLayout->addWidget(deviceGroup);

    // === DEVELOPER DISK IMAGES ===
    auto *diskImageGroup = new QGroupBox("Developer Disk Images");
    auto *diskImageLayout = new QVBoxLayout(diskImageGroup);

    // Auto-mount compatible images
    m_autoMountImages =
        new QCheckBox("Automatically mount compatible developer disk images");
    diskImageLayout->addWidget(m_autoMountImages);

    // Auto-download missing images
    m_autoDownloadImages =
        new QCheckBox("Automatically download missing compatible images");
    diskImageLayout->addWidget(m_autoDownloadImages);

    // Verify image signatures
    m_verifySignatures =
        new QCheckBox("Verify image signatures before mounting");
    diskImageLayout->addWidget(m_verifySignatures);

    scrollLayout->addWidget(diskImageGroup);

    // === FILE OPERATIONS ===
    auto *fileGroup = new QGroupBox("File Operations");
    auto *fileLayout = new QVBoxLayout(fileGroup);

    // Show hidden files
    m_showHiddenFiles = new QCheckBox("Show hidden files and folders");
    fileLayout->addWidget(m_showHiddenFiles);

    // Confirm file deletions
    m_confirmDeletions = new QCheckBox("Confirm before deleting files");
    fileLayout->addWidget(m_confirmDeletions);

    // Max concurrent downloads
    auto *downloadsLayout = new QHBoxLayout();
    downloadsLayout->addWidget(new QLabel("Maximum concurrent downloads:"));
    m_maxDownloads = new QSpinBox();
    m_maxDownloads->setRange(1, 10);
    downloadsLayout->addWidget(m_maxDownloads);
    downloadsLayout->addStretch();
    fileLayout->addLayout(downloadsLayout);

    scrollLayout->addWidget(fileGroup);

    // === ADVANCED SETTINGS ===
    auto *advancedGroup = new QGroupBox("Advanced");
    auto *advancedLayout = new QVBoxLayout(advancedGroup);

    // Debug logging
    m_enableDebugLogging = new QCheckBox("Enable debug logging");
    advancedLayout->addWidget(m_enableDebugLogging);

    // Keep log files
    auto *logLayout = new QHBoxLayout();
    logLayout->addWidget(new QLabel("Keep log files for:"));
    m_logRetention = new QSpinBox();
    m_logRetention->setRange(1, 365);
    m_logRetention->setSuffix(" days");
    logLayout->addWidget(m_logRetention);
    logLayout->addStretch();
    advancedLayout->addLayout(logLayout);

    // Expert mode
    m_expertMode = new QCheckBox("Enable expert mode (shows advanced options)");
    advancedLayout->addWidget(m_expertMode);

    scrollLayout->addWidget(advancedGroup);

    // Add stretch to push everything to the top
    scrollLayout->addStretch();

    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameStyle(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // == BUTTONS ===
    auto *buttonLayout = new QHBoxLayout();

    m_checkUpdatesButton = new QPushButton("Check for Updates");
    m_resetButton = new QPushButton("Reset Settings");
    m_applyButton = new QPushButton("Apply");

    buttonLayout->addWidget(m_checkUpdatesButton);
    // buttonLayout->addStretch();
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->setContentsMargins(10, 10, 10, 10);

    mainLayout->addWidget(scrollArea);
    mainLayout->addLayout(buttonLayout);

    // Connect button signals
    connect(m_checkUpdatesButton, &QPushButton::clicked, this,
            &SettingsWidget::onCheckUpdatesClicked);
    connect(m_resetButton, &QPushButton::clicked, this,
            &SettingsWidget::onResetToDefaultsClicked);
    connect(m_applyButton, &QPushButton::clicked, this,
            &SettingsWidget::onApplyClicked);
}

void SettingsWidget::loadSettings()
{
    // TODO: Load from SettingsManager
    // m_downloadPathEdit->setText(SettingsManager::sharedInstance()->downloadPath());
    // m_autoUpdateCheck->setChecked(SettingsManager::sharedInstance()->autoCheckUpdates());
    // etc...
}

void SettingsWidget::connectSignals()
{
    // Connect all checkboxes and combos for immediate feedback
    connect(m_autoUpdateCheck, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWidget::onSettingChanged);
    connect(m_connectionTimeout, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsWidget::onSettingChanged);
    connect(m_autoDetectDevices, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_showDeviceNotifications, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_autoMountImages, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_autoDownloadImages, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_verifySignatures, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_showHiddenFiles, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_confirmDeletions, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_maxDownloads, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SettingsWidget::onSettingChanged);
    connect(m_enableDebugLogging, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_logRetention, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &SettingsWidget::onSettingChanged);
    connect(m_expertMode, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
}

void SettingsWidget::onBrowseButtonClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Download Directory", m_downloadPathEdit->text(),
        QFileDialog::ShowDirsOnly);

    if (!dir.isEmpty()) {
        m_downloadPathEdit->setText(dir);
        onSettingChanged();
    }
}

void SettingsWidget::onCheckUpdatesClicked()
{
    // TODO: Implement update checking logic
    m_checkUpdatesButton->setText("Checking...");
    m_checkUpdatesButton->setEnabled(false);

    // Simulate check (replace with actual update check)
    QTimer::singleShot(2000, this, [this]() {
        m_checkUpdatesButton->setText("Check for Updates");
        m_checkUpdatesButton->setEnabled(true);
        QMessageBox::information(this, "Updates",
                                 "You are running the latest version.");
    });
}

void SettingsWidget::onResetToDefaultsClicked()
{
    auto reply = QMessageBox::question(
        this, "Reset Settings",
        "Are you sure you want to reset all settings to their default values?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetToDefaults();
    }
}

void SettingsWidget::onApplyClicked()
{
    saveSettings();
    QMessageBox::information(this, "Settings", "Settings have been applied.");
}

void SettingsWidget::onSettingChanged()
{
    // Enable apply button when settings change
    m_applyButton->setEnabled(true);
}

void SettingsWidget::saveSettings()
{
    // TODO: Save to SettingsManager
    // SettingsManager::sharedInstance()->setDownloadPath(m_downloadPathEdit->text());
    // SettingsManager::sharedInstance()->setAutoCheckUpdates(m_autoUpdateCheck->isChecked());
    // etc...

    m_applyButton->setEnabled(false);
}

void SettingsWidget::resetToDefaults()
{
    // TODO: Reset all controls to default values
    // m_downloadPathEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    // m_autoUpdateCheck->setChecked(true);
    // etc...

    onSettingChanged();
}
