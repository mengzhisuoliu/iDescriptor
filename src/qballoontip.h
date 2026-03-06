#ifndef QBALLOONTIP_H
#define QBALLOONTIP_H

#include "iDescriptor-ui.h"
#include <QBasicTimer>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QWidget>

class QBalloonTip : public QWidget
{
    Q_OBJECT
public:
    explicit QBalloonTip(QWidget *widget);
    void hideBalloon();
    bool isBalloonVisible();
    void updateBalloonPosition(const QPoint &pos);
    void toggleBaloon(const QPoint &pos, int timeout, bool forceVisible);
    void balloon(const QPoint &, int msecs);
    ZIconWidget *getButton() { return m_button; }
    ZIconWidget *m_button =
        new ZIconWidget(QIcon(":/resources/icons/UimProcess.png"), "Processes");

signals:
    void messageClicked();

protected:
    ~QBalloonTip();
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void timerEvent(QTimerEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QWidget *widget;
    QBasicTimer timer;
    bool m_visible = false;
};
#endif // QBALLOONTIP_H