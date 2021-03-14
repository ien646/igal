#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QKeyEvent>
#include <QMovie>
#include <QMediaPlayer>
#include <QMediaPlaylist>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <thread>

#ifdef WIN32
const std::wstring CACHE_DIR = L".igal_cache";
const std::wstring DIR_SEPARATOR = L"/";
#define FSSTR(str) L##str
#define FSSYSTEM(arg) _wsystem(arg)
#define FSQSTRING(str) QString::fromStdWString(str)
#define OS_VID_FMT L".wmv"
#else
const std::string CACHE_DIR = ".igal_cache";
const std::wstring DIR_SEPARATOR = "/";
#define FSSTR(str) str.
#define FSSYSTEM(arg) system(arg)
#define FSQSTRING(str) QString::fromStdString(str)
#define OS_VID_FMT ".avi"
#endif

const std::set<std::string> validExtensions = {
    ".jpg",".jpeg", ".png", ".gif", ".mp4", ".webm"
};

const std::set<std::string> imageExtensions = {
    ".jpg",".jpeg", ".png"
};

const std::set<std::string> animationExtensions = {
    ".png", ".gif"
};

const std::set<std::string> videoExtensions = {
    ".mp4", ".webm", ".wmv", ".avi"
};

size_t genLargeRand()
{
    size_t result = 0;
    for (int i = 0; i < 5; ++i) 
    {
        result = (result << 15) | (rand() & 0x7FFF);
    }
    return result & 0xFFFFFFFFFFFFFFFFull;
}

fs_str_t getTargetDirectory(const fs_str_t& target)
{
    #ifdef WIN32
    return std::filesystem::path(target).remove_filename().wstring();
    #else
    return std::filesystem::path(target).remove_filename().string();
    #endif
}

fs_str_t getTargetFilename(const fs_str_t& target)
{
    #ifdef WIN32
    return std::filesystem::path(target).filename().wstring();
    #else
    return std::filesystem::path(target).filename().string();
    #endif
}

MainWindow::MainWindow(const fs_str_t& target, QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    target(target),
    currentDir(getTargetDirectory(target))
{
    ui->setupUi(this);
    resizeTimer.setSingleShot(true);
    connect(&resizeTimer, SIGNAL(timeout()), SLOT(resizeEnd()));

    srand(std::chrono::system_clock::now().time_since_epoch().count());

    player = new QMediaPlayer();
    playlist = new QMediaPlaylist(player);
    video = new QVideoWidget();

    centralWidget()->layout()->addWidget(video);
    video->setContentsMargins(0, 0, 0, 0);
    video->show();

    player->setVideoOutput(video);
    player->setVolume(50);
    player->setPlaylist(playlist);

    playlist->setPlaybackMode(QMediaPlaylist::PlaybackMode::Loop);

    setWindowFlags(windowFlags() | Qt::CustomizeWindowHint |
        Qt::WindowMinimizeButtonHint |
        Qt::WindowMaximizeButtonHint |
        Qt::WindowCloseButtonHint);

    loadItem();    

    std::thread([&]()
    {
        setupItemList();
        itemListReady = true;

        loadSurroundingNext();
        loadSurroundingPrev();
    }).detach();
}

void MainWindow::keyPressEvent(QKeyEvent* e)
{
    QMainWindow::keyPressEvent(e);
    switch (e->key())
    {
    case 'f':
    case 'F':
        if (isFullScreen())
            showNormal();
        else        
            showFullScreen();
        break;

    case 'm':
    case 'M':
        if (video->isVisible())
        {
            if (player->volume() == 0)
            {
                player->setVolume(50);
            }
            else
            {
                player->setVolume(0);
            }
        }
        break;

    case 'r':
    case 'R':
        itemListIndex = genLargeRand() % itemList.size();
        target = itemList[itemListIndex];
        loadItem();
        loadSurroundingPrev();
        loadSurroundingNext();
        break;

    case Qt::Key_PageUp:        
        if (itemListIndex <= 10)
        {
            itemListIndex = 0;
        }
        else
        {
            itemListIndex -= 10;
        }
        target = itemList[itemListIndex];
        loadItem();
        loadSurroundingPrev();
        loadSurroundingNext();
        break;

    case Qt::Key_PageDown:
        if (itemListIndex >= itemList.size() - 1 - 10)
        {
            itemListIndex = itemList.size() - 1;
        }
        else
        {
            itemListIndex += 10;
        }
        target = itemList[itemListIndex];
        loadItem();
        loadSurroundingPrev();
        loadSurroundingNext();
        break;

    case Qt::Key_Escape:
        if (isFullScreen())
            showNormal();
        break;

    case Qt::Key_Left:
        if (e->modifiers().testFlag(Qt::KeyboardModifier::ControlModifier))
        {
            if (player->position() <= 1000)
            {
                player->setPosition(0);
            }
            else
            {
                player->setPosition(player->position() - 1000);
            }            
        }
        else
        {
            previousItem();
        }
        break;

    case Qt::Key_Right:
        if (e->modifiers().testFlag(Qt::KeyboardModifier::ControlModifier))
        {
            player->setPosition(player->position() + 1000);
        }
        else
        {
            nextItem();
        }
        
        break;

    case Qt::Key_Home:
        firstItem();
        break;

    case Qt::Key_End:
        lastItem();
        break;

    default:
        break;
    }
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    if (!video->isVisible())
    {
        ui->image_view->setPixmap(ui->image_view->pixmap()->scaled(size(), Qt::KeepAspectRatio));
        resizeTimer.start(200);
    }
    QWidget::resizeEvent(e);
}

void MainWindow::resizeEnd()
{
    if (!video->isVisible())
    {
        reloadTarget();
    }
}

void initCacheDir(const fs_str_t& target)
{
    auto tpath = getTargetDirectory(target) + CACHE_DIR;

    if(!std::filesystem::exists(tpath))
    {
        std::filesystem::create_directory(tpath);
    }
}

bool isAnimatedPng(const fs_str_t& target)
{
    std::ifstream ifs(target, std::ios::binary);
    ifs >> std::noskipws;

    std::string data(4096, 0);
    ifs.read(data.data(), data.size());

    return data.find("acTL") != std::string::npos;
}

fs_str_t genFfmpegCmd(const fs_str_t& source, const fs_str_t& target)
{
    return FSSTR("ffmpeg -y -i ") + source + FSSTR(" -b:v 8M ") + target;
}
    
fs_str_t getCachedAnimatedPath(const fs_str_t& target)
{
    auto cached_path = getTargetDirectory(target) + CACHE_DIR + DIR_SEPARATOR + getTargetFilename(target) + OS_VID_FMT;

    if (!std::filesystem::exists(cached_path))
    {
        initCacheDir(target);

        fs_str_t ffmpeg_cmd_mp4 = genFfmpegCmd(target, cached_path);
        FSSYSTEM(ffmpeg_cmd_mp4.c_str());

        bool ok = std::filesystem::exists(target);
    }

    return cached_path;
}

void MainWindow::playVideo(const fs_str_t& vpath)
{
    ui->image_view->setVisible(false);
    ui->image_view->setPixmap(QPixmap());

    video->setVisible(true);

    auto media = QUrl::fromLocalFile(FSQSTRING(vpath.c_str()));
    
    playlist->clear();
    playlist->addMedia(media);
    playlist->setCurrentIndex(0);

    if (!playlist->isEmpty())
    {
        player->play();
    }
}

void MainWindow::playImage(const fs_str_t& ipath)
{
    player->stop();
    playlist->clear();

    video->setVisible(false);

    QPixmap currentPixmap(FSQSTRING(ipath));        

    ui->image_view->setVisible(true);
    ui->image_view->setPixmap(currentPixmap.scaled(size(), Qt::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation));
}

void MainWindow::playImage(QPixmap* pixmap)
{
    player->stop();
    playlist->clear();

    video->setVisible(false);

    ui->image_view->setVisible(true);
    ui->image_view->setPixmap(pixmap->scaled(size(), Qt::KeepAspectRatio, Qt::TransformationMode::SmoothTransformation));
}

bool isImage(const fs_str_t& target)
{
    std::string ext = std::filesystem::path(target).extension().string();
    if (imageExtensions.count(ext))
    {
        if (ext != ".png")
        {
            return true;
        }
        else
        {
            return !isAnimatedPng(target);
        }
    }
    return false;
}

bool isAnimation(const fs_str_t& target)
{
    std::string ext = std::filesystem::path(target).extension().string();
    if (animationExtensions.count(ext))
    {
        if (ext != ".png")
        {
            return true;
        }
        else
        {
            return isAnimatedPng(target);
        }
    }
    return false;
}

bool isVideo(const fs_str_t& target)
{
    std::string ext = std::filesystem::path(target).extension().string();
    return videoExtensions.count(ext);
}

void MainWindow::loadImage(QPixmap* pixmap)
{
    playImage(pixmap);  
}

void MainWindow::loadItem()
{
    setWindowTitle(FSQSTRING(getTargetFilename(target)));

    std::string ext = std::filesystem::path(target).extension().string();

    if (isAnimation(target))
    {
        playVideo(getCachedAnimatedPath(target));
    }
    else if (isImage(target))
    {
        playImage(target);
    }
    else if (isVideo(target))
    {
        playVideo(target);
    }
}

void MainWindow::loadSurroundingPrev()
{
    surroundingPrevReady = false;
    if (itemListIndex != 0)
    {
        std::thread([&, idx = itemListIndex]()
        {
            const auto& prevTarget = itemList[idx - 1];
            prevName = getTargetFilename(prevTarget);
            std::string ext = std::filesystem::path(prevTarget).extension().string();

            if ((ext == ".png" && isAnimatedPng(prevTarget)) || imageExtensions.count(ext))
            {
                surroundingPrev = QPixmap(FSQSTRING(prevTarget));
                surroundingPrevReady = true;
            }
        }).detach();
    }
}

void MainWindow::loadSurroundingNext()
{
    surroundingNextReady = false;

    if (itemListIndex < itemList.size() - 1)
    {
        std::thread([&, idx = itemListIndex]()
        {
            const auto& nextTarget = itemList[idx + 1];
            nextName = getTargetFilename(nextTarget);
            std::string ext = std::filesystem::path(nextTarget).extension().string();

            if ((ext == ".png" && isAnimatedPng(nextTarget)) || imageExtensions.count(ext))
            {
                surroundingNext = QPixmap(FSQSTRING(nextTarget));
                surroundingNextReady = true;
            }
        }).detach();
    }
}

void MainWindow::previousItem()
{
    if (!itemListReady || itemListIndex == 0)
    {
        return;
    }

    --itemListIndex;
    if (surroundingPrevReady && !video->isVisible())
    {
        surroundingNext = *ui->image_view->pixmap();
        setWindowTitle(FSQSTRING(prevName));
        loadImage(&(*surroundingPrev));
    }
    else
    {
        reloadTarget();
        loadSurroundingNext();        
    }
    loadSurroundingPrev();
}

void MainWindow::nextItem()
{
    if (!itemListReady || (itemListIndex) == itemList.size() - 1)
    {
        return;
    }
    ++itemListIndex;
    if (surroundingNextReady && !video->isVisible())
    {
        surroundingPrev = *ui->image_view->pixmap();
        setWindowTitle(FSQSTRING(nextName));
        loadImage(&(*surroundingNext));
    }
    else
    {
        reloadTarget();
        loadSurroundingPrev();
    }
    loadSurroundingNext();
}

void MainWindow::firstItem()
{
    if (!itemListReady)
    {
        return;
    }
    itemListIndex = 0;
    reloadTarget();
}

void MainWindow::lastItem()
{
    if (!itemListReady)
    {
        return;
    }
    itemListIndex = itemList.size() - 1;
    reloadTarget();
}

void MainWindow::reloadTarget()
{
    target = itemList[itemListIndex];
    loadItem();
}

void MainWindow::setupItemList()
{
    namespace stdfs = std::filesystem;
    std::multimap<long long, stdfs::path> paths;
    for (auto f : stdfs::directory_iterator(currentDir))
    {
        if (stdfs::is_regular_file(f) 
            && f.path().has_extension() 
            && validExtensions.count(f.path().extension().string()))
        {
            paths.insert({ stdfs::last_write_time(f).time_since_epoch().count(), f.path() });
        }
    }

    itemList.reserve(paths.size());

    for (auto it = paths.rbegin(); it != paths.rend(); ++it)
    {
        #ifdef WIN32
        itemList.push_back(it->second.wstring());
        #else
        itemList.push_back(it->second.string());
        #endif         
    }

    auto pos = std::find(itemList.begin(), itemList.end(), target) - itemList.begin();
    itemListIndex = pos;
}

MainWindow::~MainWindow()
{
    delete ui;
}
