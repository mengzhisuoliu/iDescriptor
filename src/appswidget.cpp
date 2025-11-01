#include "appswidget.h"
#include "appcontext.h"
#include "appdownloadbasedialog.h"
#include "appdownloaddialog.h"
#include "appinstalldialog.h"
#include "appstoremanager.h"
#include "iDescriptor-ui.h"
#include "logindialog.h"
#include "sponsorwidget.h"
#include "zlineedit.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

// Helper struct for semantic version comparison
struct AppVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;

    static AppVersion fromString(QString versionString)
    {
        // Keep only digits and dots for comparison
        versionString.remove(QRegularExpression("[^\\d.]"));
        AppVersion v;
        QStringList parts = versionString.split('.');
        if (parts.size() > 0)
            v.major = parts[0].toInt();
        if (parts.size() > 1)
            v.minor = parts[1].toInt();
        if (parts.size() > 2)
            v.patch = parts[2].toInt();
        return v;
    }

    bool operator<(const AppVersion &other) const
    {
        if (major != other.major)
            return major < other.major;
        if (minor != other.minor)
            return minor < other.minor;
        return patch < other.patch;
    }

    bool operator==(const AppVersion &other) const
    {
        return major == other.major && minor == other.minor &&
               patch == other.patch;
    }

    bool operator>(const AppVersion &other) const
    {
        return !(*this < other || *this == other);
    }
    bool operator<=(const AppVersion &other) const
    {
        return (*this < other || *this == other);
    }
    bool operator>=(const AppVersion &other) const { return !(*this < other); }
};

// Checks if the current app version matches a given version condition
bool versionMatches(const QString &currentVersionStr,
                    const QString &conditionStr)
{
    AppVersion currentVersion = AppVersion::fromString(currentVersionStr);
    AppVersion conditionVersion = AppVersion::fromString(conditionStr);

    if (conditionStr.startsWith("<="))
        return currentVersion <= conditionVersion;
    if (conditionStr.startsWith(">="))
        return currentVersion >= conditionVersion;
    if (conditionStr.startsWith("<"))
        return currentVersion < conditionVersion;
    if (conditionStr.startsWith(">"))
        return currentVersion > conditionVersion;

    // Exact match
    return currentVersion == conditionVersion;
}

QJsonObject getVersionedConfig(const QJsonObject &rootObj)
{
    QStringList keys = rootObj.keys();
    for (const QString &key : keys) {
        if (versionMatches(APP_VERSION, key)) {
            return rootObj[key].toObject();
        }
    }
}

// watch for login and logout events
AppsWidget *AppsWidget::sharedInstance()
{
    static AppsWidget instance;
    return &instance;
}

AppsWidget::AppsWidget(QWidget *parent) : QWidget(parent), m_isLoggedIn(false)
{
    m_debounceTimer = new QTimer(this);
    setupUI();
}

void AppsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    // Header with login
    m_networkManager = new QNetworkAccessManager(this);

    QWidget *headerWidget = new QWidget();
    headerWidget->setFixedHeight(60);
    headerWidget->setStyleSheet("border-bottom: 1px solid #363d32;");

    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(20, 10, 20, 10);

    // Create status label first
    m_statusLabel = new QLabel("Not signed in");
    m_statusLabel->setStyleSheet("margin-right: 20px;");

    m_loginButton = new QPushButton();
    m_searchEdit = new ZLineEdit();
    m_searchEdit->setMaximumWidth(350);

    // --- Status and Login Button ---
    m_manager = AppStoreManager::sharedInstance();
    if (!m_manager) {
        qDebug() << "AppStoreManager failed to initialize";
        m_statusLabel->setText("Failed to initialize");
        m_loginButton->setText("Failed to initialize");
        m_loginButton->setEnabled(false);
        m_loginButton->setStyleSheet(
            "background-color: #ccc; color: #666; border: none; border-radius: "
            "4px; padding: 8px 16px; font-size: 14px;");
    } else {
        onAppStoreInitialized(m_manager->getAccountInfo());
    }

    m_statusLabel->setStyleSheet("font-size: 14px; color: #666;");

    mainLayout->addWidget(headerWidget);

    static ZIcon searchIcon(":/resources/icons/MdiLightMagnify.png");
    m_searchAction = m_searchEdit->addAction(
        searchIcon.getThemedPixmap(QSize(16, 16), palette()),
        QLineEdit::TrailingPosition);
    m_searchAction->setToolTip("Search");
    connect(m_searchAction, &QAction::triggered, this,
            &AppsWidget::performSearch);

    // Update search icon when theme changes
    connect(qApp, &QApplication::paletteChanged, this, [this]() {
        m_searchAction->setIcon(
            searchIcon.getThemedPixmap(QSize(16, 16), palette()));
    });

    headerLayout->addWidget(m_searchEdit);
    headerLayout->addStretch();
    headerLayout->addWidget(m_statusLabel);
    headerLayout->addWidget(m_loginButton);

    // Stacked widget for different pages
    m_stackedWidget = new QStackedWidget();
    setupDefaultAppsPage();
    setupLoadingPage();
    setupErrorPage();

    mainLayout->addWidget(m_stackedWidget);

    // Show default apps initially
    showLoading("Loading apps...");
    // Connections
    connect(m_loginButton, &QPushButton::clicked, this,
            &AppsWidget::onLoginClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            &AppsWidget::onSearchTextChanged);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this,
            &AppsWidget::performSearch);
    connect(m_manager, &AppStoreManager::loginSuccessful, this,
            &AppsWidget::onAppStoreInitialized);
    connect(m_manager, &AppStoreManager::loggedOut, this,
            &AppsWidget::onAppStoreInitialized);

    // fetch sponsors
    QUrl sponsorsUrl("http://localhost:5173/sponsors.json");
    QNetworkRequest request(sponsorsUrl);
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        try {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
                if (jsonDoc.isNull() || !jsonDoc.isObject()) {
                    qDebug() << "Failed to parse sponsors JSON";
                    showDefaultApps();
                    reply->deleteLater();
                    return;
                }

                QJsonObject rootObj = jsonDoc.object();
                QJsonObject versioned = getVersionedConfig(rootObj);

                if (versioned.isEmpty()) {
                    qDebug() << "No sponsor configuration found for version"
                             << APP_VERSION << "or default.";
                    showDefaultApps();
                    reply->deleteLater();
                    return;
                }

                QJsonObject sponsorObj = versioned["sponsors"].toObject();
                QJsonObject platinumObj = sponsorObj["platinum"].toObject();
                QJsonObject goldObj = sponsorObj["gold"].toObject();
                QJsonObject silverObj = sponsorObj["silver"].toObject();
                QJsonObject bronzeObj = sponsorObj["bronze"].toObject();

                // Store the platinum members to be used when populating the
                // grid
                m_platinumSponsors = platinumObj["members"].toArray();
                m_goldSponsors = goldObj["members"].toArray();
                m_silverSponsors = silverObj["members"].toArray();
                m_bronzeSponsors = bronzeObj["members"].toArray();

                if (!m_platinumSponsors.isEmpty()) {
                    qDebug() << "Platinum Sponsors found";
                }
            }
            qDebug() << "Sponsors fetch completed";
            showDefaultApps();
            reply->deleteLater();
        } catch (...) {
            qDebug() << "Exception occurred while processing sponsors";
            showDefaultApps();
            reply->deleteLater();
        }
    });
}

void AppsWidget::onAppStoreInitialized(const QJsonObject &accountInfo)
{
    if (accountInfo.contains("success") &&
        accountInfo.value("success").toBool()) {
        if (accountInfo.contains("email")) {
            QString email = accountInfo.value("email").toString();
            m_statusLabel->setText("Signed in as " + email);
            m_isLoggedIn = true;
        } else {
            m_statusLabel->setText("Not signed in");
        }
    } else {
        m_statusLabel->setText("Not signed in");
    }

    m_loginButton->setText(m_isLoggedIn ? "Sign Out" : "Sign In");
    m_loginButton->setStyleSheet(
        "background-color: #007AFF; color: white; border: none; "
        "border-radius: "
        "4px; padding: 8px 16px; font-size: 14px;");
    m_searchEdit->setPlaceholderText(m_isLoggedIn ? "Search for apps..."
                                                  : "Sign in to search");
}

void AppsWidget::setupDefaultAppsPage()
{
    m_defaultAppsPage = new QWidget();

    // Scroll area for apps
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }");
    m_scrollArea->viewport()->setStyleSheet("background: transparent;");

    m_contentWidget = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(m_contentWidget);
    gridLayout->setContentsMargins(20, 20, 20, 20);
    gridLayout->setSpacing(20);

    m_scrollArea->setWidget(m_contentWidget);

    QVBoxLayout *pageLayout = new QVBoxLayout(m_defaultAppsPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(m_scrollArea);

    m_stackedWidget->addWidget(m_defaultAppsPage);
}

void AppsWidget::setupLoadingPage()
{
    m_loadingPage = new QWidget();

    QVBoxLayout *loadingLayout = new QVBoxLayout(m_loadingPage);
    loadingLayout->setAlignment(Qt::AlignCenter);

    m_loadingIndicator = new QProcessIndicator();
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(64, 32);

    m_loadingLabel = new QLabel("Loading...");
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet(
        "font-size: 16px; color: #666; margin-top: 20px;");

    loadingLayout->addWidget(m_loadingIndicator, 0, Qt::AlignCenter);
    loadingLayout->addWidget(m_loadingLabel, 0, Qt::AlignCenter);

    m_stackedWidget->addWidget(m_loadingPage);
}

void AppsWidget::setupErrorPage()
{
    m_errorPage = new QWidget();

    QVBoxLayout *errorLayout = new QVBoxLayout(m_errorPage);
    errorLayout->setAlignment(Qt::AlignCenter);

    m_errorLabel = new QLabel("Error occurred");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet("font-size: 16px; color: #666;");

    errorLayout->addWidget(m_errorLabel, 0, Qt::AlignCenter);

    m_stackedWidget->addWidget(m_errorPage);
}

void AppsWidget::showDefaultApps()
{
    clearAppGrid();
    populateDefaultApps();
    m_stackedWidget->setCurrentWidget(m_defaultAppsPage);
}

void AppsWidget::showLoading(const QString &message)
{
    m_loadingLabel->setText(message);
    m_loadingIndicator->start();
    m_stackedWidget->setCurrentWidget(m_loadingPage);
}

void AppsWidget::showError(const QString &message)
{
    m_loadingIndicator->stop();
    m_errorLabel->setText(message);
    m_stackedWidget->setCurrentWidget(m_errorPage);
}

void AppsWidget::populateDefaultApps()
{
    QGridLayout *gridLayout =
        qobject_cast<QGridLayout *>(m_contentWidget->layout());
    if (!gridLayout)
        return;

    int row = 0;
    int col = 0;
    const int maxCols = 3;

    // Helper lambda to advance the grid position
    auto advanceGridPos = [&]() {
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    };

    for (const QJsonValue &sponsorValue : m_platinumSponsors) {
        QJsonObject sponsorObj = sponsorValue.toObject();
        QString name = sponsorObj.value("name").toString();
        QString bundleId = sponsorObj.value("bundleId").toString();
        QString logoUrl = sponsorObj.value("logo").toString();
        QString description = sponsorObj.value("description").toString();
        QString url = sponsorObj.value("url").toString();

        createAppCard(name, bundleId, description, logoUrl, url, gridLayout,
                      row, col, SponsorType(SponsorType::Platinum));
        advanceGridPos();
    }

    for (const QJsonValue &sponsorValue : m_goldSponsors) {
        QJsonObject sponsorObj = sponsorValue.toObject();
        QString name = sponsorObj.value("name").toString();
        QString bundleId = sponsorObj.value("bundleId").toString();
        QString description = sponsorObj.value("description").toString();
        QString logoUrl = sponsorObj.value("logo").toString();
        QString url = sponsorObj.value("url").toString();
        createAppCard(name, bundleId, description, logoUrl, url, gridLayout,
                      row, col, SponsorType(SponsorType::Gold));
        advanceGridPos();
    }

    if (m_platinumSponsors.empty() && m_goldSponsors.empty()) {
        createSponsorCard(gridLayout, row, col);
        advanceGridPos();
    }

    createAppCard("Instagram", "com.burbn.instagram",
                  "Photo & Video sharing social network", "", "", gridLayout,
                  row, col);
    advanceGridPos();
    createAppCard("Spotify", "com.spotify.client",
                  "Music streaming and podcast platform", "", "", gridLayout,
                  row, col);
    advanceGridPos();
    createAppCard("YouTube", "com.google.ios.youtube",
                  "Video sharing and streaming platform", "", "", gridLayout,
                  row, col);
    advanceGridPos();
    createAppCard("X", "com.atebits.Tweetie2", "Social media and microblogging",
                  "", "", gridLayout, row, col);
    advanceGridPos();
    createAppCard("TikTok", "com.zhiliaoapp.musically",
                  "Short-form video hosting service", "", "", gridLayout, row,
                  col);
    advanceGridPos();
    createAppCard("Twitch", "tv.twitch", "Live streaming platform", "", "",
                  gridLayout, row, col);
    advanceGridPos();
    createAppCard("Telegram", "ph.telegra.Telegraph",
                  "Cloud-based instant messaging", "", "", gridLayout, row,
                  col);
    advanceGridPos();
    createAppCard("Reddit", "com.reddit.Reddit",
                  "Social news aggregation platform", "", "", gridLayout, row,
                  col);
    advanceGridPos();

    for (const QJsonValue &sponsorValue : m_silverSponsors) {
        QJsonObject sponsorObj = sponsorValue.toObject();
        QString name = sponsorObj.value("name").toString();
        QString bundleId = sponsorObj.value("bundleId").toString();
        QString description = sponsorObj.value("description").toString();
        QString url = sponsorObj.value("url").toString();

        createAppCard(name, bundleId, description, "", url, gridLayout, row,
                      col, SponsorType(SponsorType::Silver));
        advanceGridPos();
    }

    for (const QJsonValue &sponsorValue : m_bronzeSponsors) {
        QJsonObject sponsorObj = sponsorValue.toObject();
        QString name = sponsorObj.value("name").toString();
        QString bundleId = sponsorObj.value("bundleId").toString();
        QString description = sponsorObj.value("description").toString();
        QString url = sponsorObj.value("url").toString();
        createAppCard(name, bundleId, description, "", url, gridLayout, row,
                      col, SponsorType(SponsorType::Bronze));
        advanceGridPos();
    }
    gridLayout->setRowStretch(gridLayout->rowCount(), 1);
}

void AppsWidget::clearAppGrid()
{
    QGridLayout *gridLayout =
        qobject_cast<QGridLayout *>(m_contentWidget->layout());
    if (!gridLayout)
        return;

    QLayoutItem *item;
    while ((item = gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void AppsWidget::createSponsorCard(QGridLayout *gridLayout, int row, int col)
{
    if (!gridLayout)
        return;

    ClickableWidget *sponsorCard = new ClickableWidget();
    sponsorCard->setStyleSheet("border: 1px solid #ddd; border-radius: 8px;");
    sponsorCard->setCursor(Qt::PointingHandCursor);
    connect(sponsorCard, &ClickableWidget::clicked, this, [this]() {
        auto sWidget = new SponsorWidget();
        sWidget->setAttribute(Qt::WA_DeleteOnClose);
        sWidget->show();
    });
    QVBoxLayout *sponsorLayout = new QVBoxLayout(sponsorCard);
    sponsorLayout->setContentsMargins(12, 12, 12, 12);
    sponsorLayout->setSpacing(8);

    QLabel *sponsorLabel = new QLabel("Become a Sponsor!");
    sponsorLabel->setAlignment(Qt::AlignCenter);
    sponsorLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    sponsorLayout->addWidget(sponsorLabel);

    gridLayout->addWidget(sponsorCard, row, col);
}

void AppsWidget::createAppCard(const QString &name, const QString &bundleId,
                               const QString &description,
                               const QString &logoUrl,
                               const QString &websiteUrl,
                               QGridLayout *gridLayout, int row, int col,
                               const SponsorType &sponsorType)
{
    QWidget *cardWidget = new QWidget();

    QHBoxLayout *cardLayout = new QHBoxLayout(cardWidget);
    cardLayout->setContentsMargins(15, 15, 15, 15);
    cardLayout->setSpacing(10);

    // App icon
    QLabel *iconLabel = new QLabel();
    QPixmap placeholderIcon = QApplication::style()
                                  ->standardIcon(QStyle::SP_ComputerIcon)
                                  .pixmap(64, 64);
    iconLabel->setPixmap(placeholderIcon);
    iconLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(iconLabel);

    // If logoUrl is provided and bundleId is empty, use the logo directly
    if (!logoUrl.isEmpty()) {
        QUrl url(logoUrl);
        QNetworkRequest request(url);
        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, [reply, iconLabel]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                QPixmap pixmap;
                if (pixmap.loadFromData(data)) {
                    QPixmap scaled =
                        pixmap.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
                    QPixmap rounded(64, 64);
                    rounded.fill(Qt::transparent);

                    QPainter painter(&rounded);
                    painter.setRenderHint(QPainter::Antialiasing);
                    QPainterPath path;
                    path.addRoundedRect(QRectF(0, 0, 64, 64), 16, 16);
                    painter.setClipPath(path);
                    painter.drawPixmap(0, 0, scaled);
                    painter.end();

                    iconLabel->setPixmap(rounded);
                }
            }
            reply->deleteLater();
        });
    } else if (!bundleId.isEmpty()) {
        // Use Apple's API for app icons
        fetchAppIconFromApple(
            bundleId,
            [iconLabel](const QPixmap &pixmap) {
                if (!pixmap.isNull()) {
                    QPixmap scaled =
                        pixmap.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
                    QPixmap rounded(64, 64);
                    rounded.fill(Qt::transparent);

                    QPainter painter(&rounded);
                    painter.setRenderHint(QPainter::Antialiasing);
                    QPainterPath path;
                    path.addRoundedRect(QRectF(0, 0, 64, 64), 16, 16);
                    painter.setClipPath(path);
                    painter.drawPixmap(0, 0, scaled);
                    painter.end();

                    iconLabel->setPixmap(rounded);
                }
            },
            cardWidget);
    }

    // Vertical layout for name and description
    QVBoxLayout *textLayout = new QVBoxLayout();

    // App name with sponsor indicator
    QHBoxLayout *nameLayout = new QHBoxLayout();
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size: 16px;");
    nameLabel->setWordWrap(true);
    nameLayout->addWidget(nameLabel);

    // Add sponsor type indicator
    if (!sponsorType.isEmpty()) {
        QLabel *sponsorLabel = new QLabel(sponsorType.name);
        QString textColor = (sponsorType.level == SponsorType::Platinum ||
                             sponsorType.level == SponsorType::Silver)
                                ? "#333"
                                : "white";
        sponsorLabel->setStyleSheet(QString("font-size: 10px; "
                                            "font-weight: bold; "
                                            "color: %1; "
                                            "background-color: %2; "
                                            "border-radius: 4px; "
                                            "padding: 2px 6px; "
                                            "margin-left: 8px;")
                                        .arg(textColor, sponsorType.color));
        sponsorLabel->setAlignment(Qt::AlignCenter);
        nameLayout->addWidget(sponsorLabel);
    }

    nameLayout->addStretch();
    textLayout->addLayout(nameLayout);

    // App description
    QLabel *descLabel = new QLabel(description);
    descLabel->setStyleSheet("font-size: 12px; color: #666;");
    descLabel->setAlignment(Qt::AlignLeft);
    descLabel->setWordWrap(true);
    textLayout->addWidget(descLabel);

    cardLayout->addLayout(textLayout);

    QVBoxLayout *buttonsLayout = new QVBoxLayout();

    // Install button placeholder
    if (!bundleId.isEmpty()) {
        ZLabel *installLabel = new ZLabel("Install");
        installLabel->setAlignment(Qt::AlignCenter);
        installLabel->setStyleSheet(
            "font-size: 12px; color: #007AFF; font-weight: "
            "bold; background-color: transparent;");
        installLabel->setCursor(Qt::PointingHandCursor);
        installLabel->setFixedHeight(30);

        buttonsLayout->addStretch();
        buttonsLayout->addWidget(installLabel);
        connect(installLabel, &ZLabel::clicked, this,
                [this, name, bundleId, description]() {
                    onAppCardClicked(name, bundleId, description);
                });
    }
    if (websiteUrl.isEmpty()) {
        ZLabel *downloadIpaLabel = new ZLabel("Download IPA");
        downloadIpaLabel->setAlignment(Qt::AlignCenter);
        downloadIpaLabel->setStyleSheet("font-size: 12px; font-weight: "
                                        "bold; background-color: transparent;");
        downloadIpaLabel->setCursor(Qt::PointingHandCursor);

        connect(
            downloadIpaLabel, &ZLabel::clicked, this,
            [this, name, bundleId]() { onDownloadIpaClicked(name, bundleId); });

        buttonsLayout->addWidget(downloadIpaLabel);
    } else {
        ZLabel *websiteLabel = new ZLabel("Website");
        websiteLabel->setStyleSheet("font-size: 12px; font-weight: "
                                    "bold; background-color: transparent;");
        websiteLabel->setAlignment(Qt::AlignCenter);
        websiteLabel->setCursor(Qt::PointingHandCursor);

        connect(websiteLabel, &ZLabel::clicked, this, [this, websiteUrl]() {
            QDesktopServices::openUrl(QUrl(websiteUrl));
        });
        buttonsLayout->addWidget(websiteLabel);
    }

    buttonsLayout->addStretch();

    cardLayout->addLayout(buttonsLayout);
    gridLayout->addWidget(cardWidget, row, col);
}
void AppsWidget::onDownloadIpaClicked(const QString &name,
                                      const QString &bundleId)
{
    QString description = "Download the IPA file for " + name;
    AppDownloadDialog dialog(name, bundleId, description, this);
    dialog.exec();
}

void AppsWidget::onLoginClicked()
{
    if (m_isLoggedIn) {
        AppStoreManager *manager = AppStoreManager::sharedInstance();
        if (manager) {
            manager->revokeCredentials();
        }
        m_isLoggedIn = false;
        m_loginButton->setText("Sign In");
        m_statusLabel->setText("Not signed in");
        m_searchEdit->setPlaceholderText("Sign in to search");
        return;
    }

    LoginDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Login was successful, update UI
        AppStoreManager *manager = AppStoreManager::sharedInstance();
        if (manager) {
            QJsonObject accountInfo = manager->getAccountInfo();
            if (accountInfo.contains("success") &&
                accountInfo.value("success").toBool()) {
                if (accountInfo.contains("email")) {
                    QString email = accountInfo.value("email").toString();
                    m_statusLabel->setText("Signed in as " + email);
                    m_isLoggedIn = true;
                    m_loginButton->setText("Sign Out");
                    m_searchEdit->setPlaceholderText("Search for apps...");
                }
            }
        }
    }
}

void AppsWidget::onAppCardClicked(const QString &appName,
                                  const QString &bundleId,
                                  const QString &description)
{
    if (!m_isLoggedIn) {
        QMessageBox::information(this, "Sign In Required",
                                 "Please sign in to install apps.");
        return;
    }

    AppInstallDialog dialog(appName, description, bundleId, this);
    dialog.exec();
}

void AppsWidget::onSearchTextChanged() { m_debounceTimer->start(300); }

void AppsWidget::performSearch()
{
    QString searchTerm = m_searchEdit->text().trimmed();
    if (searchTerm.isEmpty()) {
        showDefaultApps();
        return;
    }

    showLoading(QString("Searching for \"%1\"...").arg(searchTerm));

    AppStoreManager *manager = AppStoreManager::sharedInstance();
    if (!manager) {
        showError("Failed to initialize App Store manager.");
        return;
    }

    manager->searchApps(searchTerm, 20,
                        [this](bool success, const QString &results) {
                            onSearchFinished(success, results);
                        });
}

void AppsWidget::onSearchFinished(bool success, const QString &results)
{
    // FIXME: cancel fetch instead of just ignoring results
    QString searchTerm = m_searchEdit->text().trimmed();
    if (searchTerm.isEmpty()) {
        showDefaultApps();
        return;
    }

    if (!success || results.isEmpty()) {
        showError("No apps found or search failed.");
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(results.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString()
                 << " on output: " << results;
        showError("Failed to parse search results.");
        return;
    }

    qDebug() << "Search results:" << doc;
    QJsonObject rootObj = doc.object();
    if (!rootObj.value("success").toBool()) {
        QString errorMessage =
            rootObj.value("error").toString("Unknown search error.");
        showError(QString("Search error: %1").arg(errorMessage));
        return;
    }

    QJsonArray resultsArray = rootObj.value("results").toArray();
    if (resultsArray.isEmpty()) {
        showError("No apps found.");
        return;
    }

    clearAppGrid();
    QGridLayout *gridLayout =
        qobject_cast<QGridLayout *>(m_contentWidget->layout());
    if (!gridLayout)
        return;

    int row = 0;
    int col = 0;
    const int maxCols = 3;

    for (const QJsonValue &appValue : resultsArray) {
        QJsonObject appObj = appValue.toObject();
        QString name = appObj.value("trackName").toString();
        QString bundleId = appObj.value("bundleId").toString();
        QString description = "Version: " + appObj.value("version").toString();

        createAppCard(name, bundleId, description, "", "", gridLayout, row,
                      col);

        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
    gridLayout->setRowStretch(gridLayout->rowCount(), 1);
    m_stackedWidget->setCurrentWidget(m_defaultAppsPage);
}
