#include "installedappswidget.h"
#include "iDescriptor.h"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QEnterEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QStyle>
#include <QtConcurrent/QtConcurrent>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/lockdown.h>
#include <plist/plist.h>

// AppTabWidget Implementation
AppTabWidget::AppTabWidget(const QString &appName, const QString &bundleId,
                           const QString &version, QWidget *parent)
    : QWidget(parent), m_appName(appName), m_bundleId(bundleId),
      m_version(version)
{
    setFixedHeight(60);
    setMinimumWidth(250);
    setCursor(Qt::PointingHandCursor);

    // Load placeholder icon
    m_icon = QApplication::style()
                 ->standardIcon(QStyle::SP_ComputerIcon)
                 .pixmap(32, 32);

    fetchAppIcon();
}

void AppTabWidget::fetchAppIcon()
{
    fetchAppIconFromApple(
        m_bundleId,
        [this](const QPixmap &pixmap) {
            if (!pixmap.isNull()) {
                QPixmap scaled =
                    pixmap.scaled(32, 32, Qt::KeepAspectRatioByExpanding,
                                  Qt::SmoothTransformation);
                QPixmap rounded(32, 32);
                rounded.fill(Qt::transparent);

                QPainter painter(&rounded);
                painter.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addRoundedRect(QRectF(0, 0, 32, 32), 8, 8);
                painter.setClipPath(path);
                painter.drawPixmap(0, 0, scaled);
                painter.end();

                m_icon = rounded;
                update();
            }
        },
        this);
}

void AppTabWidget::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void AppTabWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    QColor bgColor;
    if (m_selected) {
        bgColor = QColor(0, 122, 255); // #007AFF
    } else if (m_hovered) {
        bgColor = QColor(224, 224, 224); // #e0e0e0
    } else {
        bgColor = QColor(245, 245, 245); // #f5f5f5
    }

    painter.fillRect(rect().adjusted(2, 2, -2, -2), bgColor);

    // Border
    painter.setPen(QPen(QColor(204, 204, 204), 1)); // #ccc
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 4, 4);

    // Icon
    QRect iconRect(10, 14, 32, 32);
    painter.drawPixmap(iconRect, m_icon);

    // Text
    QColor textColor = m_selected ? Qt::white : Qt::black;
    painter.setPen(textColor);

    QFont font = painter.font();
    font.setWeight(QFont::Medium);
    painter.setFont(font);

    QRect textRect(50, 10, width() - 60, 20);
    QString displayText = m_appName;
    if (displayText.length() > 20) {
        displayText = displayText.left(17) + "...";
    }
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);

    // Version
    if (!m_version.isEmpty()) {
        font.setPointSize(font.pointSize() - 1);
        font.setWeight(QFont::Normal);
        painter.setFont(font);

        QColor versionColor =
            m_selected ? QColor(255, 255, 255, 180) : QColor(102, 102, 102);
        painter.setPen(versionColor);

        QRect versionRect(50, 32, width() - 60, 16);
        painter.drawText(versionRect, Qt::AlignLeft | Qt::AlignVCenter,
                         m_version);
    }
}

void AppTabWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    emit clicked();
}

void AppTabWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void AppTabWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

InstalledAppsWidget::InstalledAppsWidget(iDescriptorDevice *device,
                                         QWidget *parent)
    : QWidget(parent), m_device(device)
{
    m_watcher = new QFutureWatcher<QVariantMap>(this);
    m_containerWatcher = new QFutureWatcher<QVariantMap>(this);
    setupUI();

    connect(m_watcher, &QFutureWatcher<QVariantMap>::finished, this,
            &InstalledAppsWidget::onAppsDataReady);
    connect(m_containerWatcher, &QFutureWatcher<QVariantMap>::finished, this,
            &InstalledAppsWidget::onContainerDataReady);

    fetchInstalledApps();
}

void InstalledAppsWidget::setupUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Left side - Custom tab area with scroll
    QFrame *tabFrame = new QFrame();
    tabFrame->setMinimumWidth(350);
    tabFrame->setMaximumWidth(400);
    tabFrame->setStyleSheet("QFrame { border: 1px solid #ccc; }");

    QVBoxLayout *tabFrameLayout = new QVBoxLayout(tabFrame);
    tabFrameLayout->setContentsMargins(0, 0, 0, 0);
    tabFrameLayout->setSpacing(0);

    // Search box at the top
    QWidget *searchContainer = new QWidget();
    searchContainer->setFixedHeight(50);
    QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(10, 10, 10, 10);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Search apps...");
    m_searchEdit->setStyleSheet("QLineEdit { "
                                "    border: 2px solid #e0e0e0; "
                                "    border-radius: 6px; "
                                "    padding: 8px 12px; "
                                "    font-size: 14px; "
                                "    color: #333; "
                                "} "
                                "QLineEdit:focus { "
                                "    border: 2px solid #007AFF; "
                                "    outline: none; "
                                "} "
                                "QLineEdit::placeholder { "
                                "    color: #999; "
                                "}");

    // Add search icon
    QAction *searchAction = m_searchEdit->addAction(
        this->style()->standardIcon(QStyle::SP_FileDialogContentsView),
        QLineEdit::LeadingPosition);
    searchAction->setToolTip("Search");

    searchLayout->addWidget(m_searchEdit);
    tabFrameLayout->addWidget(searchContainer);

    // Add a separator line
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("QFrame { color: #e0e0e0; }");
    tabFrameLayout->addWidget(separator);

    m_tabScrollArea = new QScrollArea();
    m_tabScrollArea->setWidgetResizable(true);
    m_tabScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tabScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tabScrollArea->setStyleSheet("QScrollArea { border: none;  }");

    m_tabContainer = new QWidget();
    // m_tabContainer->setStyleSheet("");
    m_tabLayout = new QVBoxLayout(m_tabContainer);
    m_tabLayout->setContentsMargins(0, 0, 0, 0);
    m_tabLayout->setSpacing(0);
    m_tabLayout->addStretch(); // Push tabs to top

    m_tabScrollArea->setWidget(m_tabContainer);
    tabFrameLayout->addWidget(m_tabScrollArea);

    // Right side - Content area
    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet("border: 1px solid #ccc;");

    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(15);

    m_contentLabel = new QLabel("Select an app to view details");
    m_contentLabel->setAlignment(Qt::AlignCenter);
    m_contentLabel->setStyleSheet("font-size: 16px; color: #666;");
    contentLayout->addWidget(m_contentLabel);

    // Container explorer area
    QLabel *containerTitle = new QLabel("App Container:");
    containerTitle->setStyleSheet(
        "font-size: 14px; font-weight: bold; color: #333;");
    containerTitle->setVisible(false);
    contentLayout->addWidget(containerTitle);

    m_containerScrollArea = new QScrollArea();
    m_containerScrollArea->setWidgetResizable(true);
    m_containerScrollArea->setMinimumHeight(200);
    m_containerScrollArea->setStyleSheet(
        "QScrollArea { border: 1px solid #ddd; border-radius: 4px; }");
    m_containerScrollArea->setVisible(false);

    m_containerWidget = new QWidget();
    m_containerLayout = new QVBoxLayout(m_containerWidget);
    m_containerLayout->setContentsMargins(10, 10, 10, 10);
    m_containerLayout->setSpacing(5);

    m_containerScrollArea->setWidget(m_containerWidget);
    contentLayout->addWidget(m_containerScrollArea);

    // Progress bar for loading
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0); // Indeterminate progress
    m_progressBar->setVisible(false);
    contentLayout->addWidget(m_progressBar);

    m_mainLayout->addWidget(tabFrame);
    m_mainLayout->addWidget(m_contentWidget, 1); // Give content area more space

    // Connect search functionality
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            &InstalledAppsWidget::filterApps);

    showLoadingState();
}

void InstalledAppsWidget::showLoadingState()
{
    m_contentLabel->setText("Loading installed apps...");
    m_progressBar->setVisible(true);

    // Clear existing tabs
    qDeleteAll(m_appTabs);
    m_appTabs.clear();
    m_selectedTab = nullptr;
}

void InstalledAppsWidget::showErrorState(const QString &error)
{
    m_contentLabel->setText(QString("Error loading apps: %1").arg(error));
    m_progressBar->setVisible(false);
}

void InstalledAppsWidget::fetchInstalledApps()
{
    if (!m_device || !m_device->device) {
        showErrorState("Invalid device");
        return;
    }

    QFuture<QVariantMap> future = QtConcurrent::run([this]() -> QVariantMap {
        QVariantMap result;
        QVariantList apps;

        instproxy_client_t instproxy = nullptr;
        lockdownd_client_t lockdownClient = nullptr;
        lockdownd_service_descriptor_t lockdowndService = nullptr;

        try {
            if (lockdownd_client_new_with_handshake(
                    m_device->device, &lockdownClient, APP_LABEL) !=
                LOCKDOWN_E_SUCCESS) {
                result["error"] = "Could not connect to lockdown service";
                return result;
            }

            if (lockdownd_start_service(
                    lockdownClient, "com.apple.mobile.installation_proxy",
                    &lockdowndService) != LOCKDOWN_E_SUCCESS) {
                result["error"] = "Could not start installation proxy service";
                lockdownd_client_free(lockdownClient);
                return result;
            }

            if (instproxy_client_new(m_device->device, lockdowndService,
                                     &instproxy) != INSTPROXY_E_SUCCESS) {
                result["error"] = "Could not connect to installation proxy";
                lockdownd_service_descriptor_free(lockdowndService);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            lockdownd_service_descriptor_free(lockdowndService);
            lockdowndService = nullptr;

            // Get both User and System apps
            QStringList appTypes = {"User", "System"};

            for (const QString &appType : appTypes) {
                plist_t client_opts = plist_new_dict();
                plist_dict_set_item(
                    client_opts, "ApplicationType",
                    plist_new_string(appType.toUtf8().constData()));

                plist_t return_attrs = plist_new_array();
                plist_array_append_item(return_attrs,
                                        plist_new_string("CFBundleIdentifier"));
                plist_array_append_item(
                    return_attrs, plist_new_string("CFBundleDisplayName"));
                plist_array_append_item(
                    return_attrs,
                    plist_new_string("CFBundleShortVersionString"));
                plist_array_append_item(return_attrs,
                                        plist_new_string("CFBundleVersion"));

                plist_dict_set_item(client_opts, "ReturnAttributes",
                                    return_attrs);

                plist_t apps_plist = nullptr;
                if (instproxy_browse(instproxy, client_opts, &apps_plist) ==
                        INSTPROXY_E_SUCCESS &&
                    apps_plist) {
                    if (plist_get_node_type(apps_plist) == PLIST_ARRAY) {
                        for (uint32_t i = 0;
                             i < plist_array_get_size(apps_plist); i++) {
                            plist_t app_info =
                                plist_array_get_item(apps_plist, i);
                            if (!app_info)
                                continue;

                            QVariantMap appData;

                            // Get bundle identifier
                            plist_t bundle_id = plist_dict_get_item(
                                app_info, "CFBundleIdentifier");
                            if (bundle_id && plist_get_node_type(bundle_id) ==
                                                 PLIST_STRING) {
                                char *bundle_id_str = nullptr;
                                plist_get_string_val(bundle_id, &bundle_id_str);
                                if (bundle_id_str) {
                                    appData["bundleId"] =
                                        QString(bundle_id_str);
                                    free(bundle_id_str);
                                }
                            }

                            // Get display name
                            plist_t display_name = plist_dict_get_item(
                                app_info, "CFBundleDisplayName");
                            if (display_name &&
                                plist_get_node_type(display_name) ==
                                    PLIST_STRING) {
                                char *display_name_str = nullptr;
                                plist_get_string_val(display_name,
                                                     &display_name_str);
                                if (display_name_str) {
                                    appData["displayName"] =
                                        QString(display_name_str);
                                    free(display_name_str);
                                }
                            }

                            // Get version
                            plist_t version = plist_dict_get_item(
                                app_info, "CFBundleShortVersionString");
                            if (version &&
                                plist_get_node_type(version) == PLIST_STRING) {
                                char *version_str = nullptr;
                                plist_get_string_val(version, &version_str);
                                if (version_str) {
                                    appData["version"] = QString(version_str);
                                    free(version_str);
                                }
                            }

                            appData["type"] = appType;

                            if (!appData["bundleId"].toString().isEmpty()) {
                                apps.append(appData);
                            }
                        }
                    }
                    plist_free(apps_plist);
                }
                plist_free(client_opts);
            }

            instproxy_client_free(instproxy);
            lockdownd_client_free(lockdownClient);

            result["apps"] = apps;
            result["success"] = true;

        } catch (const std::exception &e) {
            if (instproxy)
                instproxy_client_free(instproxy);
            if (lockdownClient)
                lockdownd_client_free(lockdownClient);
            if (lockdowndService)
                lockdownd_service_descriptor_free(lockdowndService);

            result["error"] = QString("Exception: %1").arg(e.what());
        }

        return result;
    });

    m_watcher->setFuture(future);
}

void InstalledAppsWidget::onAppsDataReady()
{
    QVariantMap result = m_watcher->result();
    m_progressBar->setVisible(false);

    if (!result.value("success", false).toBool()) {
        showErrorState(result.value("error", "Unknown error").toString());
        return;
    }

    QVariantList apps = result.value("apps").toList();
    if (apps.isEmpty()) {
        m_contentLabel->setText("No apps found");
        return;
    }

    // Sort apps by display name
    std::sort(apps.begin(), apps.end(),
              [](const QVariant &a, const QVariant &b) {
                  QString nameA = a.toMap().value("displayName").toString();
                  QString nameB = b.toMap().value("displayName").toString();
                  if (nameA.isEmpty())
                      nameA = a.toMap().value("bundleId").toString();
                  if (nameB.isEmpty())
                      nameB = b.toMap().value("bundleId").toString();
                  return nameA.compare(nameB, Qt::CaseInsensitive) < 0;
              });

    // Clear existing tabs
    qDeleteAll(m_appTabs);
    m_appTabs.clear();
    m_selectedTab = nullptr;

    // Create tabs for each app
    for (const QVariant &appVariant : apps) {
        QVariantMap appData = appVariant.toMap();
        QString displayName = appData.value("displayName").toString();
        QString bundleId = appData.value("bundleId").toString();
        QString version = appData.value("version").toString();
        QString appType = appData.value("type").toString();

        if (displayName.isEmpty()) {
            displayName = bundleId;
        }

        // Create tab name with type indicator
        QString tabName = displayName;
        if (appType == "System") {
            tabName += " (System)";
        }

        createAppTab(tabName, bundleId, version);
    }

    m_contentLabel->setText(
        QString("Found %1 installed apps").arg(apps.count()));

    // Select first tab if available
    if (!m_appTabs.isEmpty()) {
        selectAppTab(m_appTabs.first());
    }
}

void InstalledAppsWidget::createAppTab(const QString &appName,
                                       const QString &bundleId,
                                       const QString &version)
{
    AppTabWidget *tabWidget =
        new AppTabWidget(appName, bundleId, version, this);
    connect(tabWidget, &AppTabWidget::clicked, this,
            &InstalledAppsWidget::onAppTabClicked);

    // Remove the stretch before adding the new tab
    m_tabLayout->removeItem(m_tabLayout->itemAt(m_tabLayout->count() - 1));

    m_tabLayout->addWidget(tabWidget);
    m_tabLayout->addStretch(); // Add stretch back at the end

    m_appTabs.append(tabWidget);
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

    // Update content
    QString bundleId = tab->getBundleId();
    QString version = tab->getVersion();
    QString appName = tab->getAppName();

    // Remove the (System) suffix for display
    QString displayName = appName;
    displayName.remove(" (System)");

    QString content = QString("<h2>%1</h2>"
                              "<p><b>Bundle ID:</b> %2</p>"
                              "<p><b>Version:</b> %3</p>"
                              "<hr>")
                          .arg(displayName, bundleId,
                               version.isEmpty() ? "Unknown" : version);

    m_contentLabel->setText(content);
    m_contentLabel->setTextFormat(Qt::RichText);
    m_contentLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_contentLabel->setWordWrap(true);

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
    if (!m_device || !m_device->device) {
        return;
    }

    // Clear previous container data
    QLayoutItem *item;
    while ((item = m_containerLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Show loading state
    QLabel *loadingLabel = new QLabel("Loading app container...");
    loadingLabel->setStyleSheet("color: #666; font-style: italic;");
    m_containerLayout->addWidget(loadingLabel);
    m_containerScrollArea->setVisible(true);

    // Find container title and make it visible
    QVBoxLayout *contentLayout =
        qobject_cast<QVBoxLayout *>(m_contentWidget->layout());
    if (contentLayout && contentLayout->count() > 1) {
        QLayoutItem *titleItem = contentLayout->itemAt(1);
        if (titleItem && titleItem->widget()) {
            titleItem->widget()->setVisible(true);
        }
    }

    QFuture<QVariantMap> future = QtConcurrent::run([this, bundleId]()
                                                        -> QVariantMap {
        QVariantMap result;

        afc_client_t afcClient = nullptr;
        lockdownd_client_t lockdownClient = nullptr;
        lockdownd_service_descriptor_t lockdowndService = nullptr;
        house_arrest_client_t houseArrestClient = nullptr;

        try {
            if (lockdownd_client_new_with_handshake(
                    m_device->device, &lockdownClient, APP_LABEL) !=
                LOCKDOWN_E_SUCCESS) {
                result["error"] = "Could not connect to lockdown service";
                return result;
            }

            if (lockdownd_start_service(
                    lockdownClient, "com.apple.mobile.house_arrest",
                    &lockdowndService) != LOCKDOWN_E_SUCCESS) {
                result["error"] = "Could not start house arrest service";
                lockdownd_client_free(lockdownClient);
                return result;
            }

            if (house_arrest_client_new(m_device->device, lockdowndService,
                                        &houseArrestClient) !=
                HOUSE_ARREST_E_SUCCESS) {
                result["error"] = "Could not connect to house arrest";
                lockdownd_service_descriptor_free(lockdowndService);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            lockdownd_service_descriptor_free(lockdowndService);
            lockdowndService = nullptr;

            // Send vendor container command
            if (house_arrest_send_command(houseArrestClient, "VendContainer",
                                          bundleId.toUtf8().constData()) !=
                HOUSE_ARREST_E_SUCCESS) {
                result["error"] = "Could not send VendContainer command";
                house_arrest_client_free(houseArrestClient);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            // Get result
            plist_t dict = nullptr;
            if (house_arrest_get_result(houseArrestClient, &dict) !=
                    HOUSE_ARREST_E_SUCCESS ||
                !dict) {
                result["error"] = "App container not available for this app";
                house_arrest_client_free(houseArrestClient);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            // Check for error in response
            plist_t error_node = plist_dict_get_item(dict, "Error");
            if (error_node) {
                char *error_str = nullptr;
                plist_get_string_val(error_node, &error_str);
                if (error_str) {
                    result["error"] =
                        QString("Container access denied: %1").arg(error_str);
                    free(error_str);
                } else {
                    result["error"] = "Container access denied";
                }
                plist_free(dict);
                house_arrest_client_free(houseArrestClient);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            plist_free(dict);

            // Get AFC client for file access
            if (afc_client_new_from_house_arrest_client(
                    houseArrestClient, &afcClient) != AFC_E_SUCCESS) {
                result["error"] =
                    "Could not create AFC client for app container";
                house_arrest_client_free(houseArrestClient);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            // List root directory contents
            char **list = nullptr;
            if (afc_read_directory(afcClient, "/", &list) != AFC_E_SUCCESS) {
                result["error"] = "Could not read app container directory";
                afc_client_free(afcClient);
                house_arrest_client_free(houseArrestClient);
                lockdownd_client_free(lockdownClient);
                return result;
            }

            QStringList files;
            if (list) {
                for (int i = 0; list[i]; i++) {
                    QString fileName = QString::fromUtf8(list[i]);
                    if (fileName != "." && fileName != "..") {
                        files.append(fileName);
                    }
                }
                afc_dictionary_free(list);
            }

            result["files"] = files;
            result["success"] = true;

            afc_client_free(afcClient);
            house_arrest_client_free(houseArrestClient);
            lockdownd_client_free(lockdownClient);

        } catch (const std::exception &e) {
            if (afcClient)
                afc_client_free(afcClient);
            if (houseArrestClient)
                house_arrest_client_free(houseArrestClient);
            if (lockdownClient)
                lockdownd_client_free(lockdownClient);
            if (lockdowndService)
                lockdownd_service_descriptor_free(lockdowndService);

            result["error"] = QString("Exception: %1").arg(e.what());
        }

        return result;
    });

    m_containerWatcher->setFuture(future);
}

void InstalledAppsWidget::onContainerDataReady()
{
    QVariantMap result = m_containerWatcher->result();

    // Clear loading state
    QLayoutItem *item;
    while ((item = m_containerLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (!result.value("success", false).toBool()) {
        QLabel *errorLabel = new QLabel("No data available for this app");
        errorLabel->setStyleSheet(
            "color: #999; font-style: italic; text-align: center;");
        errorLabel->setAlignment(Qt::AlignCenter);
        m_containerLayout->addWidget(errorLabel);
        return;
    }

    QStringList files = result.value("files").toStringList();
    if (files.isEmpty()) {
        QLabel *emptyLabel = new QLabel("App container is empty");
        emptyLabel->setStyleSheet("color: #999; font-style: italic;");
        m_containerLayout->addWidget(emptyLabel);
        return;
    }

    // Sort files/directories
    files.sort();

    // Add files/directories to the container view
    for (const QString &fileName : files) {
        QLabel *fileLabel = new QLabel();

        // Determine if it's likely a directory (simple heuristic)
        QString displayText = fileName;
        QIcon icon;
        if (!fileName.contains('.') || fileName.endsWith("/")) {
            icon = this->style()->standardIcon(QStyle::SP_DirIcon);
            displayText = "📁 " + fileName;
        } else {
            icon = this->style()->standardIcon(QStyle::SP_FileIcon);
            displayText = "📄 " + fileName;
        }

        fileLabel->setText(displayText);
        fileLabel->setStyleSheet("QLabel { "
                                 "    padding: 4px 8px; "
                                 "    border: 1px solid transparent; "
                                 "    border-radius: 3px; "
                                 "    font-family: monospace; "
                                 "    font-size: 13px; "
                                 "} "
                                 "QLabel:hover { "
                                 "    background-color: #f0f0f0; "
                                 "    border: 1px solid #ddd; "
                                 "}");
        fileLabel->setWordWrap(true);

        m_containerLayout->addWidget(fileLabel);
    }

    // Add stretch to push items to top
    m_containerLayout->addStretch();
}
