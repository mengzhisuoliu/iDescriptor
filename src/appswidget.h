#ifndef APPSWIDGET_H
#define APPSWIDGET_H

#include "appstoremanager.h"
#include "qprocessindicator.h"
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class AppsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppsWidget(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();
    void onAppCardClicked(const QString &appName, const QString &description);
    void onDownloadIpaClicked(const QString &name, const QString &bundleId);
    void onSearchTextChanged();
    void performSearch();
    void onSearchFinished(bool success, const QString &results);
    void onAppStoreInitialized(const QJsonObject &accountInfo);

private:
    void setupUI();
    void createAppCard(const QString &name, const QString &bundleId,
                       const QString &description, const QString &iconPath,
                       QGridLayout *gridLayout, int row, int col);
    void setupDefaultAppsPage();
    void setupLoadingPage();
    void setupErrorPage();
    void showDefaultApps();
    void showLoading(const QString &message = "Loading...");
    void showError(const QString &message);
    void clearAppGrid();
    void populateDefaultApps();

    QStackedWidget *m_stackedWidget;
    QWidget *m_defaultAppsPage;
    QWidget *m_loadingPage;
    QWidget *m_errorPage;
    QProcessIndicator *m_loadingIndicator;
    QLabel *m_loadingLabel;
    QLabel *m_errorLabel;
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QPushButton *m_loginButton;
    QLabel *m_statusLabel;
    bool m_isLoggedIn;
    AppStoreManager *m_manager;

    // Search
    QLineEdit *m_searchEdit;
    QTimer *m_debounceTimer;
};

#endif // APPSWIDGET_H
