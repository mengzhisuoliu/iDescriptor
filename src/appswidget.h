#ifndef APPSWIDGET_H
#define APPSWIDGET_H

#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    QString getEmail() const;
    QString getPassword() const;

private:
    QLineEdit *m_emailEdit;
    QLineEdit *m_passwordEdit;
};

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
    void onSearchFinished();

private:
    void setupUI();
    void createAppCard(const QString &name, const QString &bundleId,
                       const QString &description, const QString &iconPath,
                       QGridLayout *gridLayout, int row, int col);
    void populateDefaultApps();
    void clearAppGrid();
    void showStatusMessage(const QString &message);

    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QPushButton *m_loginButton;
    QLabel *m_statusLabel;
    bool m_isLoggedIn;

    // Search
    QLineEdit *m_searchEdit;
    QTimer *m_debounceTimer;
    QFutureWatcher<QString> *m_searchWatcher;
};

#endif // APPSWIDGET_H
