#include "appswidget.h"
#include "appcontext.h"
#include "appdownloadbasedialog.h"
#include "appdownloaddialog.h"
#include "appinstalldialog.h"
#include "appstoremanager.h"
#include "logindialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
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
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

// watch for login and logout events
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
    QWidget *headerWidget = new QWidget();
    headerWidget->setFixedHeight(60);
    headerWidget->setStyleSheet("border-bottom: 1px solid #363d32;");

    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(20, 10, 20, 10);

    QLabel *titleLabel = new QLabel("App Store");
    titleLabel->setStyleSheet(
        "font-size: 24px; font-weight: bold; color: #333;");

    // Create status label first
    m_statusLabel = new QLabel("Not signed in");
    m_statusLabel->setStyleSheet("margin-right: 20px;");

    m_loginButton = new QPushButton();
    m_searchEdit = new QLineEdit();
    m_searchEdit->setMaximumWidth(400);
    m_searchEdit->setStyleSheet("QLineEdit { "
                                "  padding: 8px; "
                                "  border: 1px solid #ccc; "
                                "  border-radius: 4px; "
                                "  font-size: 14px; "
                                "}");

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

    QAction *searchAction = m_searchEdit->addAction(
        this->style()->standardIcon(QStyle::SP_FileDialogContentsView),
        QLineEdit::TrailingPosition);
    searchAction->setToolTip("Search");
    connect(searchAction, &QAction::triggered, this,
            &AppsWidget::performSearch);

    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();
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
    showDefaultApps();
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

    // Create sample app cards
    createAppCard("Instagram", "com.burbn.instagram",
                  "Photo & Video sharing social network", "", gridLayout, 0, 0);
    createAppCard("WhatsApp", "net.whatsapp.WhatsApp",
                  "Free messaging and video calling", "", gridLayout, 0, 1);
    createAppCard("Spotify", "com.spotify.client",
                  "Music streaming and podcast platform", "", gridLayout, 0, 2);
    createAppCard("YouTube", "com.google.ios.youtube",
                  "Video sharing and streaming platform", "", gridLayout, 1, 0);
    createAppCard("X", "com.atebits.Tweetie2", "Social media and microblogging",
                  "", gridLayout, 1, 1);
    createAppCard("TikTok", "com.zhiliaoapp.musically",
                  "Short-form video hosting service", "", gridLayout, 1, 2);
    createAppCard("Twitch", "tv.twitch", "Live streaming platform", "",
                  gridLayout, 2, 0);
    createAppCard("Telegram", "ph.telegra.Telegraph",
                  "Cloud-based instant messaging", "", gridLayout, 2, 1);
    createAppCard("Reddit", "com.reddit.Reddit",
                  "Social news aggregation platform", "", gridLayout, 2, 2);

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

// void AppsWidget::showStatusMessage(const QString &message)
// {
//     showError(message);
// }

void AppsWidget::createAppCard(const QString &name, const QString &bundleId,
                               const QString &description,
                               const QString &iconPath, QGridLayout *gridLayout,
                               int row, int col)
{
    QWidget *cardWidget = new QWidget();
    // cardWidget->setFixedSize(200, 250);
    cardWidget->setCursor(Qt::PointingHandCursor);

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

    // Vertical layout for name and description
    QVBoxLayout *textLayout = new QVBoxLayout();

    // App name
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("font-size: 16px;");
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    textLayout->addWidget(nameLabel);

    // App description
    QLabel *descLabel = new QLabel(description);
    descLabel->setStyleSheet("font-size: 12px; color: #666;");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    textLayout->addWidget(descLabel);

    cardLayout->addStretch();
    cardLayout->addLayout(textLayout);

    QVBoxLayout *buttonsLayout = new QVBoxLayout();

    // Install button placeholder
    QPushButton *installLabel = new QPushButton("Install");
    QPushButton *downloadIpaLabel = new QPushButton("Download IPA");

    installLabel->setStyleSheet(
        "font-size: 12px; color: #007AFF; font-weight: bold;");
    installLabel->setCursor(Qt::PointingHandCursor);
    installLabel->setFixedHeight(30);
    // installLabel->setAlignment(Qt::AlignCenter);
    connect(
        installLabel, &QPushButton::clicked, this,
        [this, name, description]() { onAppCardClicked(name, description); });

    connect(downloadIpaLabel, &QPushButton::clicked, this,
            [this, name, bundleId]() { onDownloadIpaClicked(name, bundleId); });

    buttonsLayout->addWidget(installLabel);
    buttonsLayout->addWidget(downloadIpaLabel);

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
                                  const QString &description)
{
    if (!m_isLoggedIn) {
        QMessageBox::information(this, "Sign In Required",
                                 "Please sign in to install apps.");
        return;
    }

    AppInstallDialog dialog(appName, description, this);
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

        createAppCard(name, bundleId, description, "", gridLayout, row, col);

        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
    gridLayout->setRowStretch(gridLayout->rowCount(), 1);
    m_stackedWidget->setCurrentWidget(m_defaultAppsPage);
}
