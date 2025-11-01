#include "settingswidget.h"
#include "settingsmanager.h"
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

    // Unmount iFuse drives on exit (not implemented on macOS)
#ifndef __APPLE__
    m_unmount_iFuseDrives = new QCheckBox("Unmount iFuse drives on exit");
    generalLayout->addWidget(m_unmount_iFuseDrives);
#endif

    connect(browseButton, &QPushButton::clicked, this,
            &SettingsWidget::onBrowseButtonClicked);

    // Auto-check for updates
    m_autoUpdateCheck = new QCheckBox("Automatically check for updates");
    generalLayout->addWidget(m_autoUpdateCheck);

    m_autoRaiseWindow =
        new QCheckBox("Auto-raise main window on device connection");
    generalLayout->addWidget(m_autoRaiseWindow);

    m_switchToNewDevice = new QCheckBox("Switch to newly connected device");
    generalLayout->addWidget(m_switchToNewDevice);

    // Theme selection
    auto *themeLayout = new QHBoxLayout();
    themeLayout->addWidget(new QLabel("Theme:"));
    m_themeCombo = new QComboBox();

    /* FIXME: Theme control on Linux needs to be implemented */
#ifdef __linux__
    m_themeCombo->addItems({"System Default"});
#else
    m_themeCombo->addItems({"System Default", "Light", "Dark"});
#endif

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

    scrollLayout->addWidget(deviceGroup);

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
    SettingsManager *sm = SettingsManager::sharedInstance();

    m_downloadPathEdit->setText(sm->downloadPath());
    m_autoUpdateCheck->setChecked(sm->autoCheckUpdates());
    m_autoRaiseWindow->setChecked(sm->autoRaiseWindow());
    m_switchToNewDevice->setChecked(sm->switchToNewDevice());

#ifndef __APPLE__
    m_unmount_iFuseDrives->setChecked(sm->unmountiFuseOnExit());
#endif

    // Set theme combo box
    QString currentTheme = sm->theme();
    int themeIndex = m_themeCombo->findText(currentTheme);
    if (themeIndex != -1) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }

    m_connectionTimeout->setValue(sm->connectionTimeout());

    // Disable apply button initially
    m_applyButton->setEnabled(false);
}

void SettingsWidget::connectSignals()
{
    // Connect all checkboxes and combos for immediate feedback
    connect(m_autoUpdateCheck, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_autoRaiseWindow, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
    connect(m_switchToNewDevice, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
#ifndef __APPLE__
    connect(m_unmount_iFuseDrives, &QCheckBox::toggled, this,
            &SettingsWidget::onSettingChanged);
#endif
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWidget::onSettingChanged);
    connect(m_connectionTimeout, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsWidget::onSettingChanged);
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
    SettingsManager *sm = SettingsManager::sharedInstance();

    sm->setDownloadPath(m_downloadPathEdit->text());
    sm->setAutoCheckUpdates(m_autoUpdateCheck->isChecked());
    sm->setAutoRaiseWindow(m_autoRaiseWindow->isChecked());
    sm->setSwitchToNewDevice(m_switchToNewDevice->isChecked());

#ifndef __APPLE__
    sm->setUnmountiFuseOnExit(m_unmount_iFuseDrives->isChecked());
#endif

    sm->setTheme(m_themeCombo->currentText());
    sm->setConnectionTimeout(m_connectionTimeout->value());

    m_applyButton->setEnabled(false);
}

void SettingsWidget::resetToDefaults()
{
    SettingsManager::sharedInstance()->resetToDefaults();

    // Reload UI with default values
    loadSettings();

    onSettingChanged();
}
