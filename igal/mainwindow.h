#pragma once

#include <QtCore/qtimer.h>

#include <QtGui/qpixmap.h>

#include <QtWidgets/qlabel.h>
#include <QtWidgets/qmainwindow.h>

#include <QtMultimedia/qmediaplayer.h>
#include <QtMultimedia/qmediaplaylist.h>

#include <QtMultimediaWidgets/qvideowidget.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "defs.h"
#include "ui_mainwindow.h"

namespace Ui {
    class MainWindow;
}

struct ItemCacheEntry
{
    bool isVideo = false;
    QPixmap pixmap;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const fs_str_t& target, QWidget *parent = nullptr);   

    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void resizeEnd();

private:
    void loadItem();
    void loadImage(QPixmap* pixmap);
    void playImage(const fs_str_t& path);
    void playImage(QPixmap* pixmap);
    void playVideo(const fs_str_t& vpath);

    void previousItem();
    void nextItem();
    void loadFirstItem();
    void loadLastItem();

    void reloadTarget();

    void setupItemList();

    void loadSurroundingNext();
    void loadSurroundingPrev();

    void showVideoInfo();
    void hideVideoInfo();

    void toggleFullscreen();
    void togglePauseVideo();
    void toggleMuteVideo();
    void loadRandom();
    void skipPrev(int amount);
    void skipNext(int amount);
    void rewindVideo(int milliseconds);
    void fforwardVideo(int milliseconds);
    void increaseVideoSpeed(float v);
    void decreaseVideoSpeed(float v);
    void resetVideoSpeed();

    std::unique_ptr<Ui::MainWindow> ui;
    fs_str_t target;
    fs_str_t currentDir;
    std::unique_ptr<QMediaPlayer> player;
    std::unique_ptr<QMediaPlaylist> playlist;
    std::unique_ptr<QVideoWidget> video;
    std::unique_ptr<QLabel> videoInfoLabel;

    bool itemListReady = false;
    std::vector<fs_str_t> itemList;

    std::mutex surroundingNextMux, surroundingPrevMux;
    std::optional<QPixmap> surroundingNext;
    std::optional<QPixmap> surroundingPrev;
    std::atomic<bool> surroundingNextReady = false;
    std::atomic<bool> surroundingPrevReady = false;
    fs_str_t prevName;
    fs_str_t nextName;

    size_t itemListIndex = 0;

    QTimer resizeTimer;
};
