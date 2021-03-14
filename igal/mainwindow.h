#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QPixmap>
#include <QTimer>
#include <QVideoWidget>

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

#include "defs.h"

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
    ~MainWindow();    

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
    void firstItem();
    void lastItem();

    void reloadTarget();

    void setupItemList();

    void loadSurroundingNext();
    void loadSurroundingPrev();

    Ui::MainWindow *ui;
    fs_str_t target;
    fs_str_t currentDir;
    QMediaPlayer* player;
    QMediaPlaylist* playlist;
    QVideoWidget* video;

    bool itemListReady = false;
    std::vector<fs_str_t> itemList;

    std::mutex movingMux;
    std::optional<QPixmap> surroundingNext;
    std::optional<QPixmap> surroundingPrev;
    std::atomic<bool> surroundingNextReady = false;
    std::atomic<bool> surroundingPrevReady = false;
    fs_str_t prevName;
    fs_str_t nextName;

    size_t itemListIndex = 0;

    QTimer resizeTimer;
};

#endif // MAINWINDOW_H
