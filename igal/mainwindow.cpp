#include "mainwindow.h"

#include <QtGui/qevent.h>

#include <QtMultimedia/qmediacontent.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>

// Bullshit to deal with windows/linux handling of wstrings/utf-8 strings
#if defined(WIN32) || defined(_WIN32)
    #include "win32/utils.h"
    #define FSSTR(str) L##str
    #define FSSYSTEM(arg) execProc(arg)
    #define FSSTR_TO_QSTRING(str) QString::fromStdWString(str)
    #define QSTRING_TO_FSSTR(str) str.toStdWString()
#else    
    #define FSSTR(str) str.
    #define FSSYSTEM(arg) system(arg)
    #define FSSTR_TO_QSTRING(str) QString::fromStdString(str)
    #define QSTRING_TO_FSSTR(str) str.toStdString()
#endif

const fs_str_t CACHE_DIR = FSSTR(".igal_cache");
const fs_str_t DIR_SEPARATOR = FSSTR("/");
const fs_str_t OS_VID_FMT = FSSTR(".mp4");

const std::set<fs_str_t> validExtensions = {
    FSSTR(".jpg"),
    FSSTR(".jpeg"), 
    FSSTR(".png"),
    FSSTR(".gif"), 
    FSSTR(".mp4"),
    FSSTR(".webm"),
    FSSTR(".avi"),
#if defined(WIN32) || defined(_WIN32)
    FSSTR(".wmv"),
#endif
};

const std::set<fs_str_t> imageExtensions = {
    FSSTR(".jpg"),
    FSSTR(".jpeg"), 
    FSSTR(".png")
};

const std::set<fs_str_t> animationExtensions = {
    FSSTR(".png"), 
    FSSTR(".gif")
};

const std::set<fs_str_t> videoExtensions = {
    FSSTR(".mp4"), 
    FSSTR(".webm"),     
    FSSTR(".avi")

#if defined(WIN32) || defined(_WIN32)
    FSSTR(".wmv"),
#endif
};

fs_str_t fsStrToLower(const fs_str_t& src)
{
    return QSTRING_TO_FSSTR(FSSTR_TO_QSTRING(src).toLower());
}

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
    #if defined(WIN32) || defined(_WIN32)
        return std::filesystem::path(target).remove_filename().wstring();
    #else
        return std::filesystem::path(target).remove_filename().string();
    #endif
}

fs_str_t getTargetFilename(const fs_str_t& target)
{
    return std::filesystem::path(target).filename();
}

fs_str_t getTargetExtension(const fs_str_t& target)
{
    return fsStrToLower(std::filesystem::path(target).extension());
}

MainWindow::MainWindow(const fs_str_t& target, QWidget* parent) :
    QMainWindow(parent),
    ui(std::make_unique<Ui::MainWindow>()),
    target(target),
    currentDir(getTargetDirectory(target))
{
    ui->setupUi(this);
    resizeTimer.setSingleShot(true);
    connect(&resizeTimer, SIGNAL(timeout()), SLOT(resizeEnd()));

    srand(std::chrono::system_clock::now().time_since_epoch().count());

    player = std::make_unique<QMediaPlayer>();
    playlist = std::make_unique<QMediaPlaylist>(player.get());
    video = std::make_unique<QVideoWidget>();

    QFont videoInfoFont("Courier New");
    videoInfoFont.setBold(true);
    videoInfoFont.setPixelSize(12);

    videoInfoLabel = std::make_unique<QLabel>(this);
    videoInfoLabel->setStyleSheet("color: #EEEEEE; background: transparent");
    videoInfoLabel->setAutoFillBackground(false);
    videoInfoLabel->setFont(videoInfoFont);
    videoInfoLabel->hide();

    centralWidget()->layout()->addWidget(video.get());
    video->setContentsMargins(0, 0, 0, 0);
    video->show();

    player->setVideoOutput(video.get());
    player->setVolume(50);
    player->setPlaylist(playlist.get());

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

std::string formatVideoPosition(qint64 posMs, qint64 totalMs)
{
    qint64 posSecondsAbs = posMs / 1000;
    qint64 totalSecondsAbs = totalMs / 1000;

    qint64 posSeconds = posSecondsAbs % 60;
    qint64 posMinutes = (posSecondsAbs / 60) % 60;
    qint64 posHours = (posSecondsAbs / 60 / 60) % 24;

    qint64 totalSeconds = totalSecondsAbs % 60;
    qint64 totalMinutes = (totalSecondsAbs / 60) % 60;
    qint64 totalHours = (totalSecondsAbs / 60 / 60) % 24;

    std::stringstream ss;
    if (totalHours)
    {
        ss << ((posHours   < 10) ? "0" + posHours   : std::to_string(posHours)) << ":"
           << ((posMinutes < 10) ? "0" + posMinutes : std::to_string(posMinutes)) << ":"
           << ((posSeconds < 10) ? "0" + posSeconds : std::to_string(posSeconds));

        ss << " / ";

        ss << ((totalHours   < 10)  ? "0" + totalHours   : std::to_string(totalHours))      << ":"
           << ((totalMinutes < 10)  ? "0" + totalMinutes : std::to_string(totalMinutes))    << ":"
           << ((totalSeconds < 10)  ? "0" + totalSeconds : std::to_string(totalSeconds));
    }
    else
    {
        ss << ((posMinutes < 10) ? "0" + std::to_string(posMinutes) : std::to_string(posMinutes)) << ":"
            << ((posSeconds < 10) ? "0" + std::to_string(posSeconds) : std::to_string(posSeconds));

        ss << " / ";

        ss << ((totalMinutes < 10) ? "0" + std::to_string(totalMinutes) : std::to_string(totalMinutes)) << ":"
           << ((totalSeconds < 10) ? "0" + std::to_string(totalSeconds) : std::to_string(totalSeconds));
    }
    return ss.str();
}

void MainWindow::showVideoInfo()
{
    if (!videoInfoLabel->isVisible())
    {
        videoInfoLabel->setVisible(true);
    }
    std::string posText = "[" + formatVideoPosition(player->position(), player->duration()) + "]";
    if (player->playbackRate() != 1)
    {
        posText += " (x" + std::to_string(player->playbackRate()) +")";
    }
    videoInfoLabel->setText(QString::fromStdString(posText));
    QFontMetricsF met(videoInfoLabel->font());    
    videoInfoLabel->setGeometry(0, 0, met.horizontalAdvance(QString::fromStdString(posText)), videoInfoLabel->font().pixelSize());
}

void MainWindow::hideVideoInfo()
{
    videoInfoLabel->setText("");
}

void MainWindow::togglePauseVideo()
{
    if (!video->isVisible())
    {
        return;
    }

    if (player->state() == QMediaPlayer::State::PausedState)
    {
        player->play();
    }
    else
    {
        player->pause();
    }
}

void MainWindow::loadRandom()
{
    itemListIndex = genLargeRand() % itemList.size();
    target = itemList[itemListIndex];
    loadItem();
    loadSurroundingPrev();
    loadSurroundingNext();
}

void MainWindow::toggleMuteVideo()
{
    if (!video->isVisible())
    {
        return;
    }

    if (player->volume() == 0)
    {
        player->setVolume(50);
    }
    else
    {
        player->setVolume(0);
    }
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
    {
        showNormal();
    }
    else
    {
        showFullScreen();
    }
}

void MainWindow::skipPrev(int amount)
{
    if (itemListIndex <= amount)
    {
        itemListIndex = 0;
    }
    else
    {
        itemListIndex -= amount;
    }
    target = itemList[itemListIndex];
    loadItem();
    loadSurroundingPrev();
    loadSurroundingNext();
}

void MainWindow::skipNext(int amount)
{
    if (itemListIndex >= itemList.size() - 1 - amount)
    {
        itemListIndex = itemList.size() - 1;
    }
    else
    {
        itemListIndex += amount;
    }
    target = itemList[itemListIndex];
    loadItem();
    loadSurroundingPrev();
    loadSurroundingNext();
}

void MainWindow::rewindVideo(int milliseconds)
{
    player->setPosition(player->position() - milliseconds);
}

void MainWindow::fforwardVideo(int milliseconds)
{
    player->setPosition(player->position() + milliseconds);
}

void MainWindow::increaseVideoSpeed(float v)
{
    player->setPlaybackRate(player->playbackRate() + v);
}

void MainWindow::decreaseVideoSpeed(float v)
{
    player->setPlaybackRate(player->playbackRate() - v);
}

void MainWindow::resetVideoSpeed()
{
    player->setPlaybackRate(1);
}

void MainWindow::keyPressEvent(QKeyEvent* e)
{
    QMainWindow::keyPressEvent(e);

    bool altPressed = e->modifiers().testFlag(Qt::KeyboardModifier::AltModifier);
    bool shiftPressed = e->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier);
    bool ctrlPressed = e->modifiers().testFlag(Qt::KeyboardModifier::ControlModifier);
    bool numpadPressed = e->modifiers().testFlag(Qt::KeyboardModifier::KeypadModifier);

    switch (e->key())
    {
    case 'f':
    case 'F':
        toggleFullscreen();
        break;

    case 'm':
    case 'M':
        toggleMuteVideo();
        break;

    case 'r':
    case 'R':
        if (altPressed)
        {
            resetVideoSpeed();
        }
        else
        {
            loadRandom();
        }        
        break;

    case 'p':
    case 'P':
        togglePauseVideo();
        break;

    case Qt::Key_PageUp:        
        skipPrev(10);
        break;

    case Qt::Key_PageDown:
        skipNext(10);
        break;

    case Qt::Key_Escape:
        // exit fullscreen
        if (isFullScreen()) 
        { 
            showNormal();
        }
        break;

    case Qt::Key_Left:
        if (ctrlPressed)
        {
            rewindVideo(e->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier) ? 5000 : 1000);
        }
        else if (altPressed)
        {
            decreaseVideoSpeed(0.05F);
        }
        else
        {
            previousItem();
        }
        break;

    case Qt::Key_Right:
        if (ctrlPressed)
        {
            fforwardVideo(e->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier) ? 5000 : 1000);
        }
        else if (altPressed)
        {
            increaseVideoSpeed(0.05F);
        }
        else
        {
            nextItem();
        }
        
        break;

    case Qt::Key_Home:
        loadFirstItem();
        break;

    case Qt::Key_End:
        loadLastItem();
        break;

    default:
        break;
    }
}

QPixmap MainWindow::getTransformedPixmap(const QPixmap* px)
{
    QPixmap result = *px;
    result.scroll(currentX, currentY, result.rect());
    return result.scaled(size() * zoom, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    if (!video->isVisible() && !ui->image_view->pixmap(Qt::ReturnByValue).isNull())
    {
        ui->image_view->setPixmap(getTransformedPixmap(ui->image_view->pixmap()));
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
    return FSSTR("ffmpeg -y -i ") + source + FSSTR(" ") + target;
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

    auto media = QUrl::fromLocalFile(FSSTR_TO_QSTRING(vpath.c_str()));
    
    playlist->clear();
    playlist->addMedia(media);
    playlist->setCurrentIndex(0);

    player->setPlaybackRate(1);

    if (!playlist->isEmpty())
    {
        player->play();
    }

    std::thread([&]() 
    {
        while (!video->isVisible()) { continue; }
        while (video->isVisible())
        {
            QMetaObject::invokeMethod(this, [&] { showVideoInfo(); });
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        QMetaObject::invokeMethod(this, [&] { hideVideoInfo(); });
    }).detach();
}

void MainWindow::playImage(const fs_str_t& ipath)
{
    player->stop();
    playlist->clear();

    video->setVisible(false);

    currentImage = QImage(FSSTR_TO_QSTRING(ipath));

    ui->image_view->setVisible(true);

    QPixmap pxmap = QPixmap::fromImage(*currentImage);
    ui->image_view->setPixmap(getTransformedPixmap(&pxmap));

    return;
}

void MainWindow::playImage(QImage* image)
{
    player->stop();
    playlist->clear();

    video->setVisible(false);

    currentImage = *image;

    ui->image_view->setVisible(true);

    QPixmap pxmap = QPixmap::fromImage(*currentImage);
    ui->image_view->setPixmap(getTransformedPixmap(&pxmap));
}

bool isImage(const fs_str_t& target)
{
    fs_str_t ext = getTargetExtension(target);
    if (imageExtensions.count(ext))
    {
        if (ext != FSSTR(".png"))
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
    fs_str_t ext = getTargetExtension(target);
    if (animationExtensions.count(ext))
    {
        if (ext != FSSTR(".png"))
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
    return videoExtensions.count(getTargetExtension(target));
}

void MainWindow::loadImage(QImage* pixmap)
{
    playImage(pixmap);  
}

void MainWindow::loadItem()
{
    setWindowTitle(FSSTR_TO_QSTRING(getTargetFilename(target)));

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
    else
    {
        exit(2);
    }
}

void MainWindow::loadSurroundingPrev()
{
    surroundingPrevReady = false;

    if (itemListIndex != 0)
    {
        std::thread([&, idx = itemListIndex]()
        {
            std::lock_guard lock(surroundingPrevMux);
            const auto& prevTarget = itemList[idx - 1];
            prevName = prevTarget;
            fs_str_t ext = getTargetExtension(target);

            if ((ext == FSSTR(".png") && !isAnimatedPng(prevTarget)) || imageExtensions.count(ext))
            {
                surroundingPrev = QImage(FSSTR_TO_QSTRING(prevTarget));
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
            std::lock_guard lock(surroundingNextMux);
            const auto& nextTarget = itemList[idx + 1];
            nextName = nextTarget;
            fs_str_t ext = getTargetExtension(target);

            if ((ext == FSSTR(".png") && !isAnimatedPng(nextTarget)) || imageExtensions.count(ext))
            {
                surroundingNext = QImage(FSSTR_TO_QSTRING(nextTarget));
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
    if (surroundingPrevReady && surroundingPrev && !surroundingPrev.value().isNull() && !video->isVisible())
    {
        surroundingNext = currentImage;
        nextName = target;

        target = prevName;
        setWindowTitle(FSSTR_TO_QSTRING(getTargetFilename(prevName)));
        loadImage(&surroundingPrev.value());
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
    if (surroundingNextReady && surroundingNext.has_value() && !surroundingNext.value().isNull() && !video->isVisible())
    {
        surroundingPrev = currentImage;
        prevName = target;

        target = nextName;
        setWindowTitle(FSSTR_TO_QSTRING(getTargetFilename(nextName)));
        loadImage(&surroundingNext.value());
    }
    else
    {
        reloadTarget();
        loadSurroundingPrev();
    }
    loadSurroundingNext();
}

void MainWindow::loadFirstItem()
{
    if (!itemListReady)
    {
        return;
    }
    itemListIndex = 0;
    reloadTarget();
    loadSurroundingNext();
}

void MainWindow::loadLastItem()
{
    if (!itemListReady)
    {
        return;
    }
    itemListIndex = itemList.size() - 1;
    reloadTarget();
    loadSurroundingPrev();
}

void MainWindow::reloadTarget()
{
    if (itemList.empty())    
    {
        loadItem();
    }
    else
    {
        target = itemList[itemListIndex];
        loadItem();
    }
}

void MainWindow::setupItemList()
{
    namespace stdfs = std::filesystem;
    std::multimap<long long, stdfs::path> paths;
    for (auto f : stdfs::directory_iterator(currentDir))
    {
        fs_str_t ext = getTargetExtension(f.path());
        if (stdfs::is_regular_file(f) 
            && f.path().has_extension() 
            && validExtensions.count(ext))
        {
            paths.insert({ stdfs::last_write_time(f).time_since_epoch().count(), f.path() });
        }
    }

    itemList.reserve(paths.size());

    for (auto it = paths.rbegin(); it != paths.rend(); ++it)
    {
        itemList.push_back(it->second);
    }

    auto pos = std::find(itemList.begin(), itemList.end(), target) - itemList.begin();
    itemListIndex = pos;
}
