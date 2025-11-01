#pragma once
#include <QAbstractButton>
#include <QApplication>
#include <QGraphicsView>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStyleOption>
#include <QWidget>

#ifdef Q_OS_MAC
#include "./platform/macos.h"
#endif

#define COLOR_GREEN QColor(0, 180, 0)    // Green
#define COLOR_ORANGE QColor(255, 140, 0) // Orange
#define COLOR_RED QColor(255, 0, 0)      // Red
#define COLOR_BLUE QColor("#2b5693")
#define COLOR_ACCENT_BLUE QColor("#0b5ed7")

// A custom QGraphicsView that keeps the content fitted with aspect ratio on
// resize
class ResponsiveGraphicsView : public QGraphicsView
{
public:
    ResponsiveGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr)
        : QGraphicsView(scene, parent)
    {
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        if (scene() && !scene()->items().isEmpty()) {
            fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
        }
        QGraphicsView::resizeEvent(event);
    }
};

class ClickableWidget : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

signals:
    void clicked();

protected:
    // On mouse release, if the click is inside the widget, emit the clicked
    // signal
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton &&
            rect().contains(event->pos())) {
            emit clicked();
        }
        QWidget::mouseReleaseEvent(event);
    }
};

class ZIcon : public QIcon
{
public:
    ZIcon() : QIcon() {}
    ZIcon(const QIcon &icon) : QIcon(icon) {}
    ZIcon(const QString &fileName) : QIcon(fileName) {}
    ZIcon(const QPixmap &pixmap) : QIcon(pixmap) {}

    QPixmap getThemedPixmap(const QSize &size, const QPalette &palette) const
    {
        QPixmap pixmap = QIcon::pixmap(size);
        if (pixmap.isNull()) {
            return pixmap;
        }

        // Get the appropriate icon color based on theme
        QColor iconColor = palette.color(QPalette::WindowText);

        // Create a colored version of the icon
        QPixmap coloredPixmap(pixmap.size());
        coloredPixmap.fill(Qt::transparent);

        QPainter iconPainter(&coloredPixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        iconPainter.drawPixmap(0, 0, pixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(coloredPixmap.rect(), iconColor);

        return coloredPixmap;
    }

    void paint(QPainter *painter, const QRect &rect,
               const QPalette &palette) const
    {
        QPixmap themedPixmap = getThemedPixmap(rect.size(), palette);
        if (!themedPixmap.isNull()) {
            painter->drawPixmap(rect, themedPixmap);
        } else {
            QIcon::paint(painter, rect);
        }
    }
};

class ZIconWidget : public QAbstractButton
{
    Q_OBJECT
public:
    ZIconWidget(const QIcon &icon, const QString &tooltip,
                QWidget *parent = nullptr)
        : QAbstractButton(parent), m_icon(icon)
    {
        setToolTip(tooltip);
        setFixedSize(32, 32);
        setIconSize(QSize(24, 24));
        setCursor(Qt::PointingHandCursor);
        connect(qApp, &QApplication::paletteChanged, this,
                [this]() { update(); });
    }

    void setIcon(const QIcon &icon)
    {
        m_icon = ZIcon(icon);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw background circle when hovered or pressed
        if (underMouse() || isDown()) {
            QColor bgColor = palette().color(QPalette::Highlight);
            bgColor.setAlpha(isDown() ? 60 : 30);
            painter.setBrush(bgColor);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(rect().adjusted(2, 2, -2, -2));
        }

        QRect iconRect = rect();
        iconRect.setSize(iconSize()); // Use iconSize() from QAbstractButton
        iconRect.moveCenter(rect().center());

        m_icon.paint(&painter, iconRect, palette());
    }

private:
    ZIcon m_icon;
};

enum class iDescriptorTool {
    Airplayer,
    RealtimeScreen,
    MountDevImage,
    VirtualLocation,
    Restart,
    Shutdown,
    RecoveryMode,
    QueryMobileGestalt,
    DeveloperDiskImages,
    WirelessPhotoImport,
    CableInfoWidget,
    /*
        TODO: to be implemented
        TouchIdTest,
        FaceIdTest,
    */
    NetworkDevices,
    iFuse,
    Unknown
};

class ModernSplitterHandle : public QSplitterHandle
{
public:
    ModernSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
        : QSplitterHandle(orientation, parent)
    {
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw fading left and right borders (no top/bottom)
        QColor borderColor = QApplication::palette().color(QPalette::Mid);

        // Create gradient for fading effect
        int fadeMargin = 20; // pixels to fade over
        int centerHeight = height() / 2;
        int fadeStart = fadeMargin;
        int fadeEnd = height() - fadeMargin;

        // Left border with fade
        for (int y = 0; y < height(); ++y) {
            QColor currentColor = borderColor;
            if (y < fadeStart) {
                // Fade from transparent to full opacity
                float alpha = static_cast<float>(y) / fadeStart;
                currentColor.setAlphaF(alpha * borderColor.alphaF());
            } else if (y > fadeEnd) {
                // Fade from full opacity to transparent
                float alpha = static_cast<float>(height() - y) / fadeMargin;
                currentColor.setAlphaF(alpha * borderColor.alphaF());
            }
            painter.setPen(QPen(currentColor, 1));
            painter.drawPoint(0, y);
        }

        // Right border with fade
        for (int y = 0; y < height(); ++y) {
            QColor currentColor = borderColor;
            if (y < fadeStart) {
                float alpha = static_cast<float>(y) / fadeStart;
                currentColor.setAlphaF(alpha * borderColor.alphaF());
            } else if (y > fadeEnd) {
                float alpha = static_cast<float>(height() - y) / fadeMargin;
                currentColor.setAlphaF(alpha * borderColor.alphaF());
            }
            painter.setPen(QPen(currentColor, 1));
            painter.drawPoint(width() - 1, y);
        }

        // Draw the center button
        QColor buttonColor = QApplication::palette().color(QPalette::Text);
        buttonColor.setAlpha(60);

        int margin = 10;
        int availableWidth = width() - (2 * margin);
        int centerX = margin + availableWidth / 2;
        int centerY = height() / 2;

        int buttonWidth = 6;
        int buttonHeight = 50;

        QRect buttonRect(centerX - buttonWidth / 2, centerY - buttonHeight / 2,
                         buttonWidth, buttonHeight);

        painter.setBrush(QBrush(buttonColor));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(buttonRect, buttonWidth / 2, buttonWidth / 2);
    }
};

class ModernSplitter : public QSplitter
{
public:
    ModernSplitter(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSplitter(orientation, parent)
    {
        setHandleWidth(10);
    }

protected:
    QSplitterHandle *createHandle() override
    {
        return new ModernSplitterHandle(orientation(), this);
    }
};

class ZLabel : public QLabel
{
    Q_OBJECT
public:
    using QLabel::QLabel;

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton &&
            rect().contains(event->pos())) {
            emit clicked();
        }
        QLabel::mouseReleaseEvent(event);
    }
};
