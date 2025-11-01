#ifndef APPSWIDGET_H
#define APPSWIDGET_H

#include "appstoremanager.h"
#include "qprocessindicator.h"
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

struct SponsorType {
    enum Level { None, Bronze, Silver, Gold, Platinum };

    Level level;
    QString name;
    QString color;

    SponsorType(Level l = None) : level(l)
    {
        switch (l) {
        case Platinum:
            name = "PLATINUM";
            color = "#E5E4E2"; // Platinum silver-white
            break;
        case Gold:
            name = "GOLD";
            color = "#FFD700"; // Gold
            break;
        case Silver:
            name = "SILVER";
            color = "#C0C0C0"; // Silver
            break;
        case Bronze:
            name = "BRONZE";
            color = "#CD7F32"; // Bronze
            break;
        default:
            name = "";
            color = "";
            break;
        }
    }

    bool isEmpty() const { return level == None; }
};

class AppsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppsWidget(QWidget *parent = nullptr);
    static AppsWidget *sharedInstance();
    void onAppCardClicked(const QString &appName, const QString &bundleId,
                          const QString &description);
private slots:
    void onLoginClicked();
    void onDownloadIpaClicked(const QString &name, const QString &bundleId);
    void onSearchTextChanged();
    void performSearch();
    void onSearchFinished(bool success, const QString &results);
    void onAppStoreInitialized(const QJsonObject &accountInfo);

private:
    void setupUI();
    void createAppCard(const QString &name, const QString &bundleId,
                       const QString &description, const QString &logoUrl,
                       const QString &websiteUrl, QGridLayout *gridLayout,
                       int row, int col,
                       const SponsorType &sponsorType = SponsorType());
    void setupDefaultAppsPage();
    void setupLoadingPage();
    void setupErrorPage();
    void showDefaultApps();
    void showLoading(const QString &message = "Loading...");
    void showError(const QString &message);
    void clearAppGrid();
    void populateDefaultApps();
    void createSponsorCard(QGridLayout *gridLayout, int row, int col);

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
    QNetworkAccessManager *m_networkManager = nullptr;

    // Search
    QLineEdit *m_searchEdit;
    QTimer *m_debounceTimer;
    QAction *m_searchAction;

    // Sponsors
    QJsonArray m_platinumSponsors;
    QJsonArray m_goldSponsors;
    QJsonArray m_silverSponsors;
    QJsonArray m_bronzeSponsors;
};

#endif // APPSWIDGET_H
