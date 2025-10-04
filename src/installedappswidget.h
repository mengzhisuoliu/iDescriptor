#ifndef INSTALLEDAPPSWIDGET_H
#define INSTALLEDAPPSWIDGET_H

#include "iDescriptor.h"
#include <QCheckBox>
#include <QEnterEvent>
#include <QFrame>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

// Custom App Tab Widget
class AppTabWidget : public QGroupBox
{
    Q_OBJECT

public:
    AppTabWidget(const QString &appName, const QString &bundleId,
                 const QString &version, QWidget *parent = nullptr);

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    QString getBundleId() const { return m_bundleId; }
    QString getAppName() const { return m_appName; }
    QString getVersion() const { return m_version; }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void fetchAppIcon();
    void setupUI();
    void updateStyles();

    QString m_appName;
    QString m_bundleId;
    QString m_version;
    bool m_selected = false;
    bool m_hovered = false;

    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    QLabel *m_versionLabel;
};

class InstalledAppsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InstalledAppsWidget(iDescriptorDevice *device,
                                 QWidget *parent = nullptr);

private slots:
    void onAppsDataReady();
    void onAppTabClicked();
    void onContainerDataReady();
    void onFileSharingFilterChanged(bool enabled);

private:
    void setupUI();
    void fetchInstalledApps();
    void createAppTab(const QString &appName, const QString &bundleId,
                      const QString &version);
    void showLoadingState();
    void showErrorState(const QString &error);
    void selectAppTab(AppTabWidget *tab);
    void filterApps(const QString &searchText);
    void loadAppContainer(const QString &bundleId);

    iDescriptorDevice *m_device;
    QHBoxLayout *m_mainLayout;
    QLineEdit *m_searchEdit;
    QCheckBox *m_fileSharingCheckBox;
    QScrollArea *m_tabScrollArea;
    QWidget *m_tabContainer;
    QVBoxLayout *m_tabLayout;
    QWidget *m_contentWidget;
    QLabel *m_contentLabel;
    QProgressBar *m_progressBar;
    QScrollArea *m_containerScrollArea;
    QWidget *m_containerWidget;
    QVBoxLayout *m_containerLayout;
    QFutureWatcher<QVariantMap> *m_watcher;
    QFutureWatcher<QVariantMap> *m_containerWatcher;
    QSplitter *m_splitter;

    // App data storage
    QList<AppTabWidget *> m_appTabs;
    AppTabWidget *m_selectedTab = nullptr;
};

#endif // INSTALLEDAPPSWIDGET_H