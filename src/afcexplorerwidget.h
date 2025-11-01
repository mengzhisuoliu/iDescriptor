#ifndef AFCEXPLORER_H
#define AFCEXPLORER_H

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QAction>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QStack>
#include <QStackedWidget>
#include <QString>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <libimobiledevice/afc.h>

class ExportManager;
class ExportProgressDialog;

class AfcExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AfcExplorerWidget(iDescriptorDevice *device = nullptr,
                               bool favEnabled = false,
                               afc_client_t afcClient = nullptr,
                               QString root = "/", QWidget *parent = nullptr);
    void navigateToPath(const QString &path);
    void goHome();
signals:
    void favoritePlaceAdded(const QString &alias, const QString &path);

private slots:
    void goBack();
    void goForward();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onAddressBarReturnPressed();
    void onFileListContextMenu(const QPoint &pos);
    void onExportClicked();
    void onImportClicked();
    void onAddToFavoritesClicked();
    void onRetryClicked();

private:
    QWidget *m_explorer;
    QWidget *m_navWidget;
    QStackedWidget *m_contentStack;
    QWidget *m_fileListWidget;
    QWidget *m_errorWidget;
    QLabel *m_errorLabel;
    QPushButton *m_retryButton;
    ZIconWidget *m_exportBtn;
    ZIconWidget *m_importBtn;
    ZIconWidget *m_addToFavoritesBtn;
    QListWidget *m_fileList;
    QStack<QString> m_history;
    QStack<QString> m_forwardHistory;
    int m_currentHistoryIndex;
    QLineEdit *m_addressBar;
    ZIconWidget *m_backButton;
    ZIconWidget *m_forwardButton;
    ZIconWidget *m_homeButton;
    ZIconWidget *m_upButton;
    ZIconWidget *m_enterButton;
    iDescriptorDevice *m_device;
    bool m_favEnabled;
    afc_client_t m_afc;
    QString m_errorMessage;
    QString m_root;

    // Export system
    ExportManager *m_exportManager;
    ExportProgressDialog *m_exportProgressDialog;

    void setupFileExplorer();
    void loadPath(const QString &path);
    void updateAddressBar(const QString &path);
    void updateNavigationButtons();
    void setErrorMessage(const QString &message);
    void showErrorState();
    void showFileListState();
    void saveFavoritePlace(const QString &path, const QString &alias);
    void openWithDesktopService(const QString &nextPath, const QString &name);

    void setupContextMenu();
    void exportSelectedFile(QListWidgetItem *item);
    void exportSelectedFile(QListWidgetItem *item, const QString &directory);
    int exportFileToPath(afc_client_t afc, const char *device_path,
                         const char *local_path);
    int importFileToDevice(afc_client_t afc, const char *device_path,
                           const char *local_path);
    void updateNavStyles();
    void updateButtonStates();
    void goUp();
};

#endif // AFCEXPLORER_H
