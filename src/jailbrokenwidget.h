#ifndef JAILBROKENWIDGET_H
#define JAILBROKENWIDGET_H

#include "iDescriptor.h"
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <libssh/libssh.h>
#include <qtermwidget6/qtermwidget.h>

class JailbrokenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit JailbrokenWidget(QWidget *parent = nullptr);
    ~JailbrokenWidget();
    void initWidget();

private slots:
    void deviceConnected(iDescriptorDevice *device);
    void onConnectSSH();
    void startSSH();
    void checkSshData();

private:
    void setupTerminal();
    void connectLibsshToTerminal();
    void disconnectSSH();

    QLabel *m_infoLabel;
    iDescriptorDevice *m_device = nullptr;
    QProcess *iproxyProcess = nullptr;

    // SSH session variables
    ssh_session m_sshSession;
    ssh_channel m_sshChannel;
    QTimer *m_sshTimer;

    // Terminal widgets
    QTermWidget *m_terminal;
    QPushButton *m_connectButton;

    bool m_isInitialized = false;
    bool m_sshConnected = false;
};

#endif // JAILBROKENWIDGET_H