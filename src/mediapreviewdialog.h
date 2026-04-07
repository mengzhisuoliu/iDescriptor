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

#ifndef MEDIAPREVIEWDIALOG_H
#define MEDIAPREVIEWDIALOG_H

#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include "zloadingwidget.h"
#include <QApplication>
#include <QAudioOutput>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>
#include <QtGlobal>

class MediaPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MediaPreviewDialog(const std::shared_ptr<iDescriptorDevice> device,
                                const QString &filePath,
                                std::optional<std::shared_ptr<CXX::HauseArrest>>
                                    hause_arrest = std::nullopt,
                                bool useAfc2 = false,
                                QWidget *parent = nullptr);
    ~MediaPreviewDialog();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
#ifdef __APPLE__
    bool event(QEvent *event) override;
#endif
private slots:
    void onImageLoaded(const QPixmap &pixmap);
    void onImageLoadFailed();
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void fitToWindow();

    // Video control slots
    void onPlayPauseClicked();
    void onStopClicked();
    void onRepeatToggled(bool enabled);
    void onTimelineValueChanged(int value);
    void onTimelinePressed();
    void onTimelineReleased();
    void onVolumeChanged(int value);
    void updateVideoProgress();
    void onMediaPlayerStateChanged();
    void onMediaPlayerDurationChanged(qint64 duration);
    void onMediaPlayerPositionChanged(qint64 position);

private:
    void setupUI();
    void setupImageView();
    void setupVideoView();
    void setupVideoControls();
    void loadMedia();
    void loadImage();
    void loadVideo();
    void zoom(double factor);
    void updateZoomStatus();
    void updateVideoTimeDisplay();
    void formatTime(qint64 milliseconds, QString &timeString);

    // Core data
    std::shared_ptr<iDescriptorDevice> m_device;
    std::optional<std::shared_ptr<CXX::HauseArrest>> m_hause_arrest;
    bool m_useAfc2;
    QString m_filePath;
    bool m_isVideo;

    // UI components
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_controlsLayout;

    // Image viewing components
    QGraphicsView *m_imageView = nullptr;
    QGraphicsScene *m_imageScene = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;

    // Video viewing components
    QVideoWidget *m_videoWidget = nullptr;
    QMediaPlayer *m_mediaPlayer = nullptr;
    // Video control components
    QHBoxLayout *m_videoControlsLayout;
    QPushButton *m_playPauseBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_repeatBtn;
    ZSlider *m_timelineSlider;
    QLabel *m_timeLabel;
    QSlider *m_volumeSlider;
    QLabel *m_volumeLabel;
    QTimer *m_progressTimer;

    // Control buttons
    QPushButton *m_zoomInBtn;
    QPushButton *m_zoomOutBtn;
    QPushButton *m_zoomResetBtn;
    QPushButton *m_fitToWindowBtn;

    // State
    double m_zoomFactor = 1.0;
    QPixmap m_originalPixmap;

    // Video state
    bool m_isRepeatEnabled = true;
    bool m_isDraggingTimeline = false;
    qint64 m_videoDuration = 0;

    ZLoadingWidget *m_loadingWidget;
};

#endif // MEDIAPREVIEWDIALOG_H
