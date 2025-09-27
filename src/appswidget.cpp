#include "appswidget.h"
#include "appcontext.h"
#include "appdownloadbasedialog.h"
#include "appdownloaddialog.h"
#include "appinstalldialog.h"
#include "libipatool-go.h"
#include "logindialog.h"
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QFuture>
#include <QFutureWatcher>
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
#include <QRegularExpression>
#include <QScrollArea>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

AppsWidget::AppsWidget(QWidget *parent) : QWidget(parent), m_isLoggedIn(false)
{
    // m_searchProcess = new QProcess(this);
    m_searchWatcher = new QFutureWatcher<QString>(this);
    m_debounceTimer = new QTimer(this);
    setupUI();
}

void AppsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header with login
    QFrame *headerFrame = new QFrame();
    headerFrame->setFixedHeight(60);
    headerFrame->setStyleSheet("border-bottom: 1px solid #dee2e6;");

    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(20, 10, 20, 10);

    QLabel *titleLabel = new QLabel("App Store");
    titleLabel->setStyleSheet(
        "font-size: 24px; font-weight: bold; color: #333;");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    // Create status label first
    m_statusLabel = new QLabel("Not signed in");
    m_statusLabel->setStyleSheet("margin-right: 20px;");

    // --- Status and Login Button ---
    // TODO: need a singleton for IpaTool
    int init_result = IpaToolInitialize();
    if (init_result != 0) {
        qDebug() << "IpaToolInitialize failed with error code:" << init_result;
        m_statusLabel->setText("Failed to initialize");
    } else {
        qDebug() << "IpaToolInitialize succeeded";
        char *accountInfoCStr = IpaToolGetAccountInfo();
        if (accountInfoCStr) {
            QString jsonAccountInfo(accountInfoCStr);
            free(accountInfoCStr);

            qDebug() << "Account info JSON:" << jsonAccountInfo;

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(
                QByteArray(jsonAccountInfo.toUtf8()), &parseError);

            if (parseError.error == QJsonParseError::NoError &&
                doc.isObject()) {
                QJsonObject jsonObj = doc.object();

                if (jsonObj.contains("success") &&
                    jsonObj.value("success").toBool()) {
                    if (jsonObj.contains("email")) {
                        QString email = jsonObj.value("email").toString();
                        m_statusLabel->setText("Signed in as " + email);
                        m_isLoggedIn = true;
                    } else {
                        m_statusLabel->setText("Not signed in");
                    }
                } else {
                    m_statusLabel->setText("Not signed in");
                }
            } else {
                qDebug() << "JSON parse error:" << parseError.errorString();
                m_statusLabel->setText("Not signed in");
            }
        } else {
            m_statusLabel->setText("Not signed in");
        }
    }
    m_statusLabel->setStyleSheet("font-size: 14px; color: #666;");
    headerLayout->addWidget(m_statusLabel);

    m_loginButton = new QPushButton(m_isLoggedIn ? "Sign Out" : "Sign In");
    m_loginButton->setStyleSheet(
        "background-color: #007AFF; color: white; border: none; border-radius: "
        "4px; padding: 8px 16px; font-size: 14px;");
    headerLayout->addWidget(m_loginButton);

    mainLayout->addWidget(headerFrame);

    // --- Search Bar ---
    QHBoxLayout *searchContainerLayout = new QHBoxLayout();
    searchContainerLayout->setContentsMargins(20, 15, 20, 15);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Search for apps...");
    m_searchEdit->setMaximumWidth(400);
    m_searchEdit->setStyleSheet("QLineEdit { "
                                "  padding: 8px; "
                                "  border: 1px solid #ccc; "
                                "  border-radius: 4px; "
                                "  font-size: 14px; "
                                "}");

    QAction *searchAction = m_searchEdit->addAction(
        this->style()->standardIcon(QStyle::SP_FileDialogContentsView),
        QLineEdit::TrailingPosition);
    searchAction->setToolTip("Search");
    connect(searchAction, &QAction::triggered, this,
            &AppsWidget::performSearch);

    searchContainerLayout->addStretch();
    searchContainerLayout->addWidget(m_searchEdit);
    searchContainerLayout->addStretch();

    mainLayout->addLayout(searchContainerLayout);

    // --- Status and Login Button ---

    // Scroll area for apps
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("border: none;");

    m_contentWidget = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(m_contentWidget);
    gridLayout->setContentsMargins(20, 20, 20, 20);
    gridLayout->setSpacing(20);

    populateDefaultApps();

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);

    // Connections
    connect(m_loginButton, &QPushButton::clicked, this,
            &AppsWidget::onLoginClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            &AppsWidget::onSearchTextChanged);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this,
            &AppsWidget::performSearch);
    connect(m_searchWatcher, &QFutureWatcher<QString>::finished, this,
            &AppsWidget::onSearchFinished);
}

void AppsWidget::populateDefaultApps()
{
    clearAppGrid();
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

void AppsWidget::showStatusMessage(const QString &message)
{
    clearAppGrid();
    QGridLayout *gridLayout =
        qobject_cast<QGridLayout *>(m_contentWidget->layout());
    if (!gridLayout)
        return;

    QLabel *statusLabel = new QLabel(message);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet("font-size: 16px; color: #666;");
    gridLayout->addWidget(statusLabel, 0, 0, 1, -1, Qt::AlignCenter);
}

void AppsWidget::createAppCard(const QString &name, const QString &bundleId,
                               const QString &description,
                               const QString &iconPath, QGridLayout *gridLayout,
                               int row, int col)
{
    QFrame *cardFrame = new QFrame();
    cardFrame->setObjectName("cardFrame");
    cardFrame->setFixedSize(200, 250);
    cardFrame->setStyleSheet("#cardFrame {"
                             "  border: 1px solid #ddd;"
                             "  border-radius: 8px;"
                             "  background-color: #fff;"
                             "}"
                             "#cardFrame:hover {"
                             "  border: 1.5px solid #007AFF;"
                             "}");
    cardFrame->setCursor(Qt::PointingHandCursor);

    QVBoxLayout *cardLayout = new QVBoxLayout(cardFrame);
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
        cardFrame);

    // App name
    QLabel *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #333;");
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    cardLayout->addWidget(nameLabel);

    // App description
    QLabel *descLabel = new QLabel(description);
    descLabel->setStyleSheet("font-size: 12px; color: #666;");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    cardLayout->addWidget(descLabel);

    cardLayout->addStretch();

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

    cardLayout->addWidget(installLabel);
    cardLayout->addWidget(downloadIpaLabel);

    // Make the entire card clickable
    // cardFrame->mousePressEvent = [this, name, description](QMouseEvent *) {
    //     onAppCardClicked(name, description);
    // };

    gridLayout->addWidget(cardFrame, row, col);
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
        IpaToolRevokeCredentials();
        m_isLoggedIn = false;
        m_loginButton->setText("Sign In");
        m_statusLabel->setText("Not signed in");
        return;
    }

    LoginDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString email = dialog.getEmail();
        QString password = dialog.getPassword();
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
    if (m_searchWatcher->isRunning()) {
        m_searchWatcher->cancel();
        m_searchWatcher->waitForFinished();
    }

    QString searchTerm = m_searchEdit->text().trimmed();
    if (searchTerm.isEmpty()) {
        populateDefaultApps();
        return;
    }

    showStatusMessage(QString("Searching for \"%1\"...").arg(searchTerm));

    auto searchFn = [searchTerm]() -> QString {
        char *resultsCStr = IpaToolSearch(searchTerm.toUtf8().data(), 20);
        qDebug() << "Search results C string:" << resultsCStr;
        if (!resultsCStr) {
            return QString();
        }
        QString results(resultsCStr);
        free(resultsCStr);
        return results;
    };
    m_searchWatcher->setFuture(QtConcurrent::run(searchFn));
}

void AppsWidget::onSearchFinished()
{
    QString jsonOutput = m_searchWatcher->result();
    if (jsonOutput.isEmpty()) {
        showStatusMessage("No apps found or search failed.");
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc =
        QJsonDocument::fromJson(jsonOutput.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString()
                 << " on output: " << jsonOutput;
        showStatusMessage("Failed to parse search results.");
        return;
    }

    QJsonObject rootObj = doc.object();
    if (!rootObj.value("success").toBool()) {
        QString errorMessage =
            rootObj.value("error").toString("Unknown search error.");
        showStatusMessage(QString("Search error: %1").arg(errorMessage));
        return;
    }

    QJsonArray resultsArray = rootObj.value("results").toArray();
    if (resultsArray.isEmpty()) {
        showStatusMessage("No apps found.");
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
}
