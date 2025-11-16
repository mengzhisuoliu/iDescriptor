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

#include "deviceimagewidget.h"
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <libimobiledevice/libimobiledevice.h>

DeviceImageWidget::DeviceImageWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_device(device)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_imageLabel = new ResponsiveQLabel(this);
    m_imageLabel->setMinimumWidth(200);
    m_imageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_imageLabel->setStyleSheet("background: transparent; border: none;");

    m_mockupName = getMockupNameFromDisplayName(
        QString::fromStdString(m_device->deviceInfo.productType));
    layout->addWidget(m_imageLabel);

    setupDeviceImage();
    m_timeUpdateTimer = new QTimer(this);
    connect(m_timeUpdateTimer, &QTimer::timeout, this,
            &DeviceImageWidget::updateTime);
    m_timeUpdateTimer->start(60000); // Update every minute

    updateTime();
}

DeviceImageWidget::~DeviceImageWidget()
{
    if (m_timeUpdateTimer) {
        m_timeUpdateTimer->stop();
    }
}

void DeviceImageWidget::setupDeviceImage()
{
    m_mockupPath = getDeviceMockupPath();
    m_wallpaperPath = getWallpaperPath();

    qDebug() << "Using mockup:" << m_mockupPath;
    qDebug() << "Using wallpaper:" << m_wallpaperPath;
}

QString DeviceImageWidget::getDeviceMockupPath() const
{
    if (m_mockupName == "iPad") {
        return QString(":/resources/ipad-mockups/ipad.png");
    }
    if (m_mockupName == "unknown") {
        return QString(":/resources/ipad-mockups/ipad.png");
    }

    return QString(":/resources/iphone-mockups/iphone-%1.png")
        .arg(m_mockupName);
}

QString DeviceImageWidget::getWallpaperPath() const
{
    int iosVersion = getIosVersionFromDevice();
    qDebug() << "Detected iOS version:" << iosVersion;
    // Map iOS version to available wallpapers
    QString wallpaperVersion;

    if (iosVersion <= 4) {
        wallpaperVersion = "ios4";
    } else if (iosVersion == 5) {
        wallpaperVersion = "ios5";
    } else if (iosVersion == 6) {
        wallpaperVersion = "ios6";
    } else if (iosVersion == 7) {
        wallpaperVersion = "ios7";
    } else if (iosVersion == 8) {
        wallpaperVersion = "ios8";
    } else if (iosVersion == 9) {
        wallpaperVersion = "ios9";
    } else if (iosVersion == 10) {
        wallpaperVersion = "ios10";
    } else if (iosVersion == 11) {
        wallpaperVersion = "ios11";
    } else if (iosVersion == 12) {
        wallpaperVersion = "ios12";
    } else if (iosVersion == 13) {
        wallpaperVersion = "ios13";
    } else if (iosVersion == 14) {
        wallpaperVersion = "ios14";
    } else if (iosVersion == 15) {
        wallpaperVersion = "ios15";
    } else if (iosVersion == 16) {
        wallpaperVersion = "ios16";
    } else if (iosVersion == 17) {
        wallpaperVersion = "ios17";
    } else if (iosVersion == 18) {
        wallpaperVersion = "ios18";
    } else if (iosVersion == 26) {
        wallpaperVersion = "ios26";
    } else {
        // For future versions, use the latest available wallpaper
        wallpaperVersion = "ios26";
    }
    return QString(":/resources/ios-wallpapers/iphone-%1.png")
        .arg(wallpaperVersion);
}

QString DeviceImageWidget::getMockupNameFromDisplayName(
    const QString &displayName) const
{
    if (displayName.contains("iPhone 16", Qt::CaseInsensitive) ||
        displayName.contains("iPhone 17", Qt::CaseInsensitive)) {
        return "16";
    } else if (displayName.contains("iPhone 15", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 14", Qt::CaseInsensitive)) {
        return "15";
    } else if (displayName.contains("iPhone X", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 11", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 12", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 13", Qt::CaseInsensitive)) {
        return "x";
    } else if (displayName.contains("iPhone 6", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 7", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 8", Qt::CaseInsensitive) ||
               displayName.contains("iPhone SE 2nd", Qt::CaseInsensitive) ||
               displayName.contains("iPhone SE 3rd", Qt::CaseInsensitive)) {
        return "6";
    } else if (displayName.contains("iPhone 5", Qt::CaseInsensitive) ||
               displayName.contains("iPhone SE", Qt::CaseInsensitive)) {
        return "5";
    } else if (displayName.contains("iPhone 4", Qt::CaseInsensitive)) {
        return "4";
    } else if (displayName.contains("iPhone 3", Qt::CaseInsensitive)) {
        return "3";
    } else if (displayName.contains("iPad", Qt::CaseInsensitive)) {
        return "iPad";
    } else {
        return "unknown";
    }
}

int DeviceImageWidget::getIosVersionFromDevice() const
{
    unsigned int version = idevice_get_device_version(m_device->device);

    if (version > 0) {
        int majorVersion = (version >> 16) & 0xFF;
        return majorVersion;
    }

    // Fallback: parse from productVersion string
    QString versionString =
        QString::fromStdString(m_device->deviceInfo.productVersion);
    QStringList parts = versionString.split('.');
    if (!parts.isEmpty()) {
        bool ok;
        int majorVersion = parts.first().toInt(&ok);
        if (ok) {
            return majorVersion;
        }
    }

    // If all else fails, return unknown version (will use ios26 wallpaper)
    return 0;
}

/*
    this method is only here to calculate the screen area
    so that wallpaper perfectly fits to the screen size
    it's costy so if you want to add a new mockup run
    through this method qDebug the result and add it to createCompositeImage
    example :     screenRect = QRect(152, 79, 195, 296);
*/
QRect DeviceImageWidget::findScreenArea(const QPixmap &mockup) const
{
    QImage image = mockup.toImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return QRect();
    }

    int width = image.width();
    int height = image.height();
    int centerX = width / 2;
    int centerY = height / 2;

    if (qAlpha(image.pixel(centerX, centerY)) != 0) {
        qWarning() << "Cannot find screen area: center pixel is not "
                      "transparent. Falling back to default.";
        return QRect(width * 0.1, height * 0.1, width * 0.8, height * 0.8);
    }

    int left = centerX;
    int right = centerX;
    int top = centerY;
    int bottom = centerY;

    // Scan left from center
    while (left > 0 && qAlpha(image.pixel(left, centerY)) == 0) {
        left--;
    }

    // Scan right from center
    while (right < width - 1 && qAlpha(image.pixel(right, centerY)) == 0) {
        right++;
    }

    // Scan up from center
    while (top > 0 && qAlpha(image.pixel(centerX, top)) == 0) {
        top--;
    }

    // Scan down from center
    while (bottom < height - 1 && qAlpha(image.pixel(centerX, bottom)) == 0) {
        bottom++;
    }

    return QRect(left + 1, top + 1, right - left - 2, bottom - top - 2);
}

QPixmap DeviceImageWidget::createCompositeImage() const
{
    QPixmap mockup(m_mockupPath);
    QPixmap wallpaper(m_wallpaperPath);

    if (mockup.isNull()) {
        qWarning() << "Failed to load mockup:" << m_mockupPath;
        return QPixmap(":/resources/iphone.png"); // Fallback
    }

    if (wallpaper.isNull()) {
        qWarning() << "Failed to load wallpaper:" << m_wallpaperPath;
        return mockup; // Return just the mockup
    }

    // Create composite with mockup dimensions
    QPixmap composite(mockup.size());
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRect screenRect;
    bool useRoundedCorners = false;
    int cornerRadius = 35;
    bool isUnknown = (m_mockupName == "unknown");

    if (m_mockupName == "3") {
        screenRect = QRect(145, 72, 209, 310);
    } else if (m_mockupName == "4") {
        screenRect = QRect(414, 181, 380, 548);
    } else if (m_mockupName == "5") {
        screenRect = QRect(27, 106, 304, 537);
    } else if (m_mockupName == "6") {
        screenRect = QRect(68, 348, 1279, 2270);
    } else if (m_mockupName == "x") {
        screenRect = QRect(245, 200, 2389, 5303);
        useRoundedCorners = true;
    } else if (m_mockupName == "15") {
        screenRect = QRect(15, 10, 337, 730);
        useRoundedCorners = true;
    } else if (m_mockupName == "16") {
        screenRect = QRect(17, 16, 333, 730);
        useRoundedCorners = true;
    } else if (m_mockupName == "iPad") {
        screenRect = QRect(30, 30, 480, 690);
    } else if (m_mockupName == "unknown") {
        screenRect = QRect(33, 36, 471, 680);
    } else {
        screenRect = QRect(mockup.width() * 0.12, mockup.height() * 0.08,
                           mockup.width() * 0.76, mockup.height() * 0.84);
    }

    QPixmap scaledWallpaper = wallpaper.scaled(
        screenRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Create a clipping path with rounded corners
    if (useRoundedCorners) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(screenRect, cornerRadius, cornerRadius);
        painter.setClipPath(clipPath);
    }

    painter.drawPixmap(screenRect, scaledWallpaper);

    if (useRoundedCorners) {
        painter.setClipping(false);
    }

    painter.drawPixmap(0, 0, mockup);

    // Draw question mark for unknown devices
    if (isUnknown) {
        QFont questionFont;
        questionFont.setFamily("SF Pro Display, Helvetica, Arial");
        int questionSize = screenRect.width() / 3;
        questionFont.setPointSize(questionSize);
        questionFont.setWeight(QFont::Bold);
        painter.setFont(questionFont);

        // Question mark shadow
        painter.setPen(QColor(0, 0, 0, 150));
        painter.drawText(screenRect.adjusted(3, 3, 3, 3), Qt::AlignCenter, "?");

        // Question mark main
        painter.setPen(QColor(255, 255, 255, 255));
        painter.drawText(screenRect, Qt::AlignCenter, "?");
    }

    QString currentTime = QDateTime::currentDateTime().toString("hh:mm");

    QFont timeFont;
    timeFont.setFamily("SF Pro Display, Helvetica, Arial");
    int fontSize = screenRect.width() / 5;
    timeFont.setPointSize(fontSize);
    timeFont.setWeight(QFont::Light);
    painter.setFont(timeFont);

    painter.setPen(QColor(255, 255, 255, 255));
    painter.drawText(screenRect, Qt::AlignCenter, currentTime);

    painter.end();
    return composite;
}

void DeviceImageWidget::updateTime()
{
    QPixmap composite = createCompositeImage();
    m_imageLabel->setPixmap(composite);
}