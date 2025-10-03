#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

class SettingsWidget : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget *parent = nullptr);

private slots:
    void onBrowseButtonClicked();
    void onCheckUpdatesClicked();
    void onResetToDefaultsClicked();
    void onApplyClicked();
    void onSettingChanged();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void connectSignals();
    void resetToDefaults();

    // UI Elements
    // General
    QLineEdit *m_downloadPathEdit;
    QCheckBox *m_autoUpdateCheck;
    QComboBox *m_themeCombo;

    // Device Connection
    QSpinBox *m_connectionTimeout;
    QCheckBox *m_autoDetectDevices;
    QCheckBox *m_showDeviceNotifications;

    // Developer Disk Images
    QCheckBox *m_autoMountImages;
    QCheckBox *m_autoDownloadImages;
    QCheckBox *m_verifySignatures;

    // File Operations
    QCheckBox *m_showHiddenFiles;
    QCheckBox *m_confirmDeletions;
    QSpinBox *m_maxDownloads;

    // Advanced
    QCheckBox *m_enableDebugLogging;
    QSpinBox *m_logRetention;
    QCheckBox *m_expertMode;

    // Buttons
    QPushButton *m_checkUpdatesButton;
    QPushButton *m_resetButton;
    QPushButton *m_applyButton;
};

#endif // SETTINGSWIDGET_H
