#ifndef DEVICETABWIDGET_H
#define DEVICETABWIDGET_H

#include <QLabel>
#include <QPushButton>
#include <QTabWidget>

class DeviceTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit DeviceTabWidget(QWidget *parent = nullptr);

    int addTabCustom(QWidget *widget, const QString &text);
    void setTabIcon(int index, const QPixmap &icon);

signals:
    void navigationButtonClicked(int tabIndex, const QString &buttonText);

private slots:
    void onCloseTab(int index);

private:
    QWidget *createTabWidget(const QString &text, int index);

protected:
    void wheelEvent(QWheelEvent *event) override;
};

#endif // DEVICETABWIDGET_H
