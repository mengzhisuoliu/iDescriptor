/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "installedappswidget.h"
#include "afcexplorerwidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "qprocessindicator.h"
#include "zlineedit.h"

AppTabWidget::AppTabWidget(const QString &appName, const QString &bundleId,
                           const QString &version, const QPixmap &icon,
                           QWidget *parent)
    : QWidget(parent), m_appName(appName), m_bundleId(bundleId),
      m_version(version), m_selected(false)
{
#ifndef WIN32
    setFixedHeight(60);
#else
    setMinimumHeight(60);
#endif
    setMinimumWidth(100);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("AppTabWidget");
    setupUI(icon);
}

void AppTabWidget::setSelected(bool selected)
{
    m_selected = selected;
    updateStyles();
}

void AppTabWidget::setupUI(const QPixmap &icon)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(10);

    m_iconLabel = new IDLoadingIconLabel(this);
    m_iconLabel->setFixedSize(32, 32);

    if (!icon.isNull()) {
        m_iconLabel->setLoadedPixmap(icon);
    }
    mainLayout->addWidget(m_iconLabel);

    // Text container
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    // App name label
    m_nameLabel = new QLabel();
    QFont nameFont = m_nameLabel->font();
    nameFont.setWeight(QFont::Medium);
    m_nameLabel->setFont(nameFont);

    QString displayText = m_appName;
    if (displayText.length() > 20) {
        displayText = displayText.left(17) + "...";
    }
    m_nameLabel->setText(displayText);
    textLayout->addWidget(m_nameLabel);

    // Version label
    if (!m_version.isEmpty()) {
        m_versionLabel = new QLabel(m_version);
        m_versionLabel->setStyleSheet("font-size: 11px;");
        textLayout->addWidget(m_versionLabel);
    } else {
        m_versionLabel = nullptr;
    }

    mainLayout->addLayout(textLayout);
    mainLayout->addStretch();

    updateStyles();
}

void AppTabWidget::setIcon(const QPixmap &icon)
{
    if (!m_iconLabel)
        return;

    if (!icon.isNull()) {
        m_iconLabel->setLoadedPixmap(icon);
    } else {
        m_iconLabel->setLoadFailed();
    }
}

void AppTabWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    emit clicked();
}

void AppTabWidget::updateStyles()
{
    QString style;
#ifndef WIN32
    QColor bgColor = isDarkMode() ? qApp->palette().color(QPalette::Light)
                                  : qApp->palette().color(QPalette::Dark);
#else
    QColor bgColor =
        isDarkMode() ? QColor(255, 255, 255, 25) : QColor(0, 0, 0, 25);
#endif
    if (m_selected) {
        style =
            "#AppTabWidget { background-color: " + COLOR_ACCENT_BLUE.name() +
            "; border-radius: "
            "10px; border : 1px solid " +
            bgColor.lighter().name() + "; }";
    } else {
        style = "#AppTabWidget { background-color: " +
                bgColor.name(QColor::HexArgb) +
                "; border-radius: 10px; border: 1px solid " +
                bgColor.lighter().name() + "; }";
    }
    // prevent infinite loop
    if (style != styleSheet()) {
        setStyleSheet(style);
    }
}

InstalledAppsWidget::InstalledAppsWidget(
    const std::shared_ptr<iDescriptorDevice> device, QWidget *parent)
    : QWidget(parent), m_device(device)
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_zloadingWidget = new ZLoadingWidget(true, this);
    rootLayout->addWidget(m_zloadingWidget);
}

void InstalledAppsWidget::init()
{
    if (m_loaded) {
        qDebug()
            << "[InstalledAppsWidget]: Already initialized, skipping init()";
        return;
    }
    m_loaded = true;

    setupUI();

    connect(m_device->service_manager,
            &CXX::ServiceManager::installed_apps_retrieved, this,
            &InstalledAppsWidget::onAppsDataReady);
    setStyleSheet("InstalledAppsWidget { background: transparent; }");
    m_device->service_manager->fetch_installed_apps();
}

InstalledAppsWidget::~InstalledAppsWidget() { cleanupHouseArrestClients(); }

void InstalledAppsWidget::setupUI()
{
    QWidget *contentContainer = new QWidget(this);
    m_mainLayout = new QHBoxLayout(contentContainer);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_zloadingWidget->setupContentWidget(contentContainer);

    // Create stacked widget for different states
    m_stackedWidget = new QStackedWidget(this);
    m_mainLayout->addWidget(m_stackedWidget);

    // Create loading widget
    createLoadingWidget();

    // Create error widget
    createErrorWidget();

    // Create content widget
    createContentWidget();

    // Start in loading state
    showLoadingState();
}

void InstalledAppsWidget::showLoadingState()
{
    m_stackedWidget->setCurrentWidget(m_loadingWidget);
}

void InstalledAppsWidget::showErrorState(const QString &error)
{
    m_errorLabel->setText(QString("Error loading apps: %1").arg(error));
    m_stackedWidget->setCurrentWidget(m_errorWidget);
}

void InstalledAppsWidget::createLoadingWidget()
{
    m_loadingWidget = new QWidget();
    QVBoxLayout *loadingLayout = new QVBoxLayout(m_loadingWidget);
    loadingLayout->setAlignment(Qt::AlignCenter);

    QProcessIndicator *spinner = new QProcessIndicator();
    spinner->setType(QProcessIndicator::line_rotate);
    spinner->setFixedSize(48, 48);
    spinner->start();
    loadingLayout->addWidget(spinner, 0, Qt::AlignCenter);

    QLabel *loadingLabel = new QLabel("Loading installed apps...");
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLabel->setStyleSheet(
        "font-size: 14px; color: #666; margin-top: 10px;");
    loadingLayout->addWidget(loadingLabel);

    m_stackedWidget->addWidget(m_loadingWidget);
}

void InstalledAppsWidget::createErrorWidget()
{
    m_errorWidget = new QWidget();
    QVBoxLayout *errorLayout = new QVBoxLayout(m_errorWidget);
    errorLayout->setAlignment(Qt::AlignCenter);

    m_errorLabel = new QLabel();
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet(
        "font-size: 14px; color: #d32f2f; margin: 20px;");
    m_errorLabel->setWordWrap(true);
    errorLayout->addWidget(m_errorLabel);

    QPushButton *retryButton = new QPushButton("Retry");
    retryButton->setFixedSize(100, 30);
    // FIXME:
    // connect(retryButton, &QPushButton::clicked, this,
    //         &InstalledAppsWidget::fetchInstalledApps);
    errorLayout->addWidget(retryButton, 0, Qt::AlignCenter);

    m_stackedWidget->addWidget(m_errorWidget);
}

void InstalledAppsWidget::createContentWidget()
{
    m_contentWidget = new QWidget();
    QHBoxLayout *contentLayout = new QHBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Create main splitter
    m_splitter = new ModernSplitter(Qt::Horizontal, m_contentWidget);
    m_splitter->setChildrenCollapsible(false);
    contentLayout->addWidget(m_splitter);

    // Left side - App list
    createLeftPanel();

    // Right side - Content area
    createRightPanel();

    // Set initial splitter sizes (400px for tabs, rest for content)
    m_splitter->setSizes({400, 600});

    // Connect signals
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            &InstalledAppsWidget::filterApps);
    connect(m_fileSharingCheckBox, &QCheckBox::toggled, this,
            &InstalledAppsWidget::onFileSharingFilterChanged);

    m_stackedWidget->addWidget(m_contentWidget);
}

void InstalledAppsWidget::onAppsDataReady(const QMap<QString, QVariant> &result)
{

    if (result.isEmpty()) {
        showErrorState("No apps found or failed to retrieve apps.");
        return;
    }

    m_stackedWidget->setCurrentWidget(m_contentWidget);
    m_zloadingWidget->stop(true);

    // Clear existing tabs
    qDeleteAll(m_appTabs);
    m_appTabs.clear();
    m_selectedTab = nullptr;
    m_iconLoadQueue.clear();
    m_iconLoading = false;

    connect(m_device->service_manager, &CXX::ServiceManager::app_icon_loaded,
            this, &InstalledAppsWidget::onAppIconLoaded);
    // Create tabs for each app
    for (const QVariant &appVariant : result) {
        // variant is json object

        // Step 3: Parse JSON
        QJsonParseError error;
        QJsonDocument doc =
            QJsonDocument::fromJson(appVariant.toString().toUtf8(), &error);

        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << error.errorString();
            // return;
            continue;
        }

        QString displayName = doc["CFBundleDisplayName"].toString();
        QString bundleId = doc["bundle_id"].toString();
        QString version = doc["CFBundleShortVersionString"].toString();
        QString appType = doc["app_type"].toString();
        bool fileSharingEnabled = doc["UIFileSharingEnabled"].toBool();

        /*
            Always fails to load Fitness app container
            even though file sharing is enabled
        */
        if (bundleId == "com.apple.Fitness") {
            continue;
        }

        // // Filter by file sharing status if checkbox is checked
        if (m_fileSharingCheckBox->isChecked() && !fileSharingEnabled) {
            continue;
        }

        if (displayName.isEmpty()) {
            displayName = bundleId;
        }

        // Create tab name with type indicator
        QString tabName = displayName;
        if (appType == "System") {
            tabName += " (System)";
        }

        createAppTab(tabName, bundleId, version, QPixmap());

        enqueueIconLoad(bundleId);

        // Select first tab if available
        m_device->service_manager->fetch_app_icon(bundleId);
    }

    if (!m_appTabs.isEmpty()) {
        selectAppTab(m_appTabs.first());
    }
}

void InstalledAppsWidget::createAppTab(const QString &appName,
                                       const QString &bundleId,
                                       const QString &version,
                                       const QPixmap &icon)
{
    AppTabWidget *tabWidget =
        new AppTabWidget(appName, bundleId, version, icon, this);
    connect(tabWidget, &AppTabWidget::clicked, this,
            &InstalledAppsWidget::onAppTabClicked);

    // Remove the stretch before adding the new tab
    m_tabLayout->removeItem(m_tabLayout->itemAt(m_tabLayout->count() - 1)); //

    m_tabLayout->addWidget(tabWidget);
    m_tabLayout->addStretch(); // Add stretch back at the end

    m_appTabs[bundleId] = tabWidget;
}

void InstalledAppsWidget::onAppTabClicked()
{
    AppTabWidget *clickedTab = qobject_cast<AppTabWidget *>(sender());
    if (clickedTab) {
        selectAppTab(clickedTab);
    }
}

void InstalledAppsWidget::selectAppTab(AppTabWidget *tab)
{
    // Deselect previous tab
    if (m_selectedTab) {
        m_selectedTab->setSelected(false);
    }

    // Select new tab
    m_selectedTab = tab;
    tab->setSelected(true);

    QString bundleId = tab->getBundleId();

    // Load app container data
    loadAppContainer(bundleId);
}

void InstalledAppsWidget::filterApps(const QString &searchText)
{
    QString lowerSearchText = searchText.toLower();

    for (AppTabWidget *tab : m_appTabs) {
        bool shouldShow = false;

        if (lowerSearchText.isEmpty()) {
            shouldShow = true;
        } else {
            // Search in app name and bundle ID
            QString appName = tab->getAppName().toLower();
            QString bundleId = tab->getBundleId().toLower();

            shouldShow = appName.contains(lowerSearchText) ||
                         bundleId.contains(lowerSearchText);
        }

        tab->setVisible(shouldShow);
    }
}

void InstalledAppsWidget::loadAppContainer(const QString &bundleId)
{
    if (!m_device || m_loadingContainer) {
        return;
    }
    m_loadingContainer = true;

    disableTabs(true);
    // Clean up previous house arrest clients before creating new ones
    cleanupHouseArrestClients();

    // Clear previous container data
    QLayoutItem *item;
    while ((item = m_containerLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Create a centered loading widget
    QWidget *loadingWidget = new QWidget();
    QVBoxLayout *loadingLayout = new QVBoxLayout(loadingWidget);
    loadingLayout->setAlignment(Qt::AlignCenter);

    QProcessIndicator *l = new QProcessIndicator();
    l->setType(QProcessIndicator::line_rotate);
    l->setFixedSize(32, 32);
    l->start();
    loadingLayout->addWidget(l, 0, Qt::AlignCenter);

    m_containerLayout->addWidget(loadingWidget);

    m_houseArrestAfcClient =
        std::make_shared<CXX::HauseArrest>(m_device->udid, bundleId);

    connect(m_houseArrestAfcClient.get(),
            &CXX::HauseArrest::init_session_finished, this,
            &InstalledAppsWidget::onContainerDataReady,
            Qt::SingleShotConnection);

    m_houseArrestAfcClient->init_session();
}

void InstalledAppsWidget::onContainerDataReady(bool success)
{
    QLayoutItem *item;
    while ((item = m_containerLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    m_loadingContainer = false;
    disableTabs(false);

    if (!success) {
        qDebug() << "Error loading app container:";
        QLabel *errorLabel = new QLabel("No data available for this app");
        errorLabel->setAlignment(Qt::AlignCenter);
        m_containerLayout->addWidget(errorLabel);
        return;
    }

    // Create AfcExplorerWidget with the house arrest AFC client
    AfcExplorerWidget *explorer = new AfcExplorerWidget(
        m_device, true, m_houseArrestAfcClient, "/Documents", this);
    explorer->setStyleSheet("border :none;");
    m_containerLayout->addWidget(explorer);
}

void InstalledAppsWidget::onAppIconLoaded(const QString &bundleId,
                                          const QByteArray &icon)
{
    qDebug() << "Icon loaded for bundle ID:" << bundleId;
    AppTabWidget *tab = m_appTabs.value(bundleId, nullptr);
    if (tab) {
        qDebug() << "Setting icon for bundle ID:" << bundleId;
        QPixmap pixmap;
        pixmap.loadFromData(icon);
        tab->setIcon(pixmap);
    }

    // startNextIconLoad();
}

void InstalledAppsWidget::onFileSharingFilterChanged(bool enabled)
{
    Q_UNUSED(enabled)
    // Refresh the apps list when filter changes
    // fetchInstalledApps();
}

void InstalledAppsWidget::cleanupHouseArrestClients()
{
    if (m_houseArrestAfcClient) {
        m_houseArrestAfcClient = nullptr;
        // delete m_houseArrestAfcClient;
    }
}

void InstalledAppsWidget::createLeftPanel()
{
    QWidget *tabWidget = new QWidget();
    tabWidget->setMinimumWidth(100);
    tabWidget->setMaximumWidth(500);

    QVBoxLayout *tabWidgetLayout = new QVBoxLayout(tabWidget);
    tabWidgetLayout->setContentsMargins(0, 0, 0, 0);
    tabWidgetLayout->setSpacing(0);

    // Search container
    QWidget *searchContainer = new QWidget();
    searchContainer->setFixedHeight(60);
    QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(5, 0, 5, 5);

    // Search box
    m_searchEdit = new ZLineEdit();
    m_searchEdit->setPlaceholderText("Search apps...");
    searchLayout->addWidget(m_searchEdit);

    // File sharing filter checkbox
    // FIXME: crash when toggled
    m_fileSharingCheckBox = new QCheckBox("Show Only File Sharing Enabled");
    m_fileSharingCheckBox->setChecked(true);
    m_fileSharingCheckBox->setStyleSheet("QCheckBox { font-size: 10px; }");
    searchLayout->addWidget(m_fileSharingCheckBox);

    tabWidgetLayout->addWidget(searchContainer);

    // App list scroll area
    m_tabScrollArea = new QScrollArea();
    m_tabScrollArea->setWidgetResizable(true);
    m_tabScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tabScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tabScrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }");
    m_tabScrollArea->viewport()->setStyleSheet("background: transparent;");

    m_tabContainer = new QWidget();
    m_tabContainer->setStyleSheet("QWidget { background: transparent; }");
    m_tabLayout = new QVBoxLayout(m_tabContainer);
    m_tabLayout->setContentsMargins(0, 0, 10, 0);
    m_tabLayout->setSpacing(10);
    m_tabLayout->addStretch();

    m_tabScrollArea->setWidget(m_tabContainer);
    tabWidgetLayout->addWidget(m_tabScrollArea);

    m_splitter->addWidget(tabWidget);
}

void InstalledAppsWidget::createRightPanel()
{
    QWidget *rightContentWidget = new QWidget();

    QVBoxLayout *contentLayout = new QVBoxLayout(rightContentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 5);
    contentLayout->setSpacing(0);

    m_containerWidget = new QWidget();
    m_containerWidget->setObjectName("containerWidget");
    m_containerWidget->setStyleSheet(
        "QWidget#containerWidget { border: none; }");
    m_containerLayout = new QVBoxLayout(m_containerWidget);
    m_containerLayout->setContentsMargins(0, 0, 0, 0);
    m_containerLayout->setSpacing(0);

    contentLayout->addWidget(m_containerWidget);

    m_splitter->addWidget(rightContentWidget);
}

void InstalledAppsWidget::disableTabs(bool disable)
{
    for (AppTabWidget *tab : m_appTabs) {
        tab->setEnabled(!disable);
    }
}

void InstalledAppsWidget::enqueueIconLoad(const QString &bundleId)
{
    if (bundleId.isEmpty())
        return;

    if (!m_iconLoadQueue.contains(bundleId)) {
        m_iconLoadQueue.enqueue(bundleId);
    }

    if (!m_iconLoading) {
        startNextIconLoad();
    }
}

// FIXME: we better use this
void InstalledAppsWidget::startNextIconLoad()
{
    // if (!m_device || QCoreApplication::closingDown()) {
    //     m_iconLoading = false;
    //     return;
    // }

    // if (m_iconLoadQueue.isEmpty()) {
    //     m_iconLoading = false;
    //     return;
    // }

    // m_iconLoading = true;
    // const QString bundleId = m_iconLoadQueue.dequeue();

    // QtConcurrent::run([this, bundleId]() {
    //     if (QCoreApplication::closingDown() || !m_device)
    //         return;

    //     QPixmap iconPixmap;

    //     {
    //         std::lock_guard<std::recursive_mutex> lock(m_device->mutex);

    //         IdeviceFfiError *err = nullptr;
    //         SpringBoardServicesClientHandle *springboardClient = nullptr;

    //         err = springboard_services_connect(m_device->provider,
    //                                            &springboardClient);
    //         if (err != nullptr) {
    //             qWarning() << "Error connecting to SpringBoard services for"
    //                        << bundleId << ":"
    //                        << QString::fromUtf8(err->message);
    //             idevice_error_free(err);
    //         } else {
    //             void *out_result = nullptr;
    //             size_t out_result_len = 0;

    //             err = springboard_services_get_icon(
    //                 springboardClient, bundleId.toUtf8().constData(),
    //                 &out_result, &out_result_len);
    //             if (err != nullptr) {
    //                 qWarning() << "Error getting icon for" << bundleId << ":"
    //                            << QString::fromUtf8(err->message);
    //                 idevice_error_free(err);
    //             } else if (out_result && out_result_len > 0) {
    //                 QByteArray byteArray(
    //                     reinterpret_cast<const char *>(out_result),
    //                     static_cast<int>(out_result_len));
    //                 QImage image;
    //                 image.loadFromData(byteArray);
    //                 iconPixmap = QPixmap::fromImage(image);
    //                 springboard_services_free_icon_result(out_result,
    //                                                       out_result_len);
    //             }

    //             springboard_services_free(springboardClient);
    //         }
    //     }

    //     QMetaObject::invokeMethod(
    //         this,
    //         [this, bundleId, iconPixmap]() {
    //             if (QCoreApplication::closingDown())
    //                 return;

    //             for (AppTabWidget *tab : m_appTabs) {
    //                 if (tab->getBundleId() == bundleId) {
    //                     tab->setIcon(iconPixmap);
    //                     break;
    //                 }
    //             }

    //             m_iconLoading = false;
    //             startNextIconLoad();
    //         },
    //         Qt::QueuedConnection);
    // });
}