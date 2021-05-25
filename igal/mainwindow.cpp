#include "mainwindow.h"

#include <QtCore/qdir.h>

#include <QtGui/qevent.h>

#include <QtMultimedia/qmediacontent.h>

#include <QtWidgets/qmessagebox.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>

// Bullshit to deal with windows/linux handling of wstrings/utf-8 strings
#if defined(WIN32) || defined(_WIN32)
    #include "win32/utils.h"
    #define FSSTR(str) L##str
    const fs_str_t DIR_SEPARATOR = FSSTR("\\");

#elif defined(_POSIX_VERSION)
    #include "posix/utils.h"
    #define FSSTR(str) str.
    const fs_str_t DIR_SEPARATOR = FSSTR("/");

#endif

const fs_str_t CACHE_DIR = FSSTR(".igal_cache");
const fs_str_t OS_VID_FMT = FSSTR(".mp4");
const fs_str_t LINKS_FILE = FSSTR("links.txt");

QString fsstrToQstring(const fs_str_t& str)
{
    #if defined(WIN32) || defined(_WIN32)
        return QString::fromStdWString(str);
    #else
        return QString::fromStdString(str);
    #endif
}

void debugMessageBox(QString title, QString text)
{
    QMessageBox msgbox;
    msgbox.setWindowTitle(title);
    msgbox.setText(text);
    msgbox.exec();
}

fs_str_t getExecutableDir()
{
    return getExeDir();
}

fs_str_t qstringToFsstr(const QString& str)
{
    #if defined(WIN32) || defined(_WIN32)
        return str.toStdWString();
    #else
        return str.toStdString();
    #endif
}

void fs_system(const fs_str_t& cmdline)
{
    #if defined(WIN32) || defined(_WIN32)
        execProc(cmdline);
    #else
        system(cmdline);
    #endif
}

const std::unordered_set<fs_str_t> imageExtensions = {
    FSSTR(".jpg"),
    FSSTR(".jpeg"),
    FSSTR(".png"),
    FSSTR(".tga"),
    FSSTR(".tiff"),
    FSSTR(".webp")
};

const std::unordered_set<fs_str_t> animationExtensions = {
    FSSTR(".png"),
    FSSTR(".gif")
};

const std::unordered_set<fs_str_t> videoExtensions = {
    FSSTR(".avi"),
    FSSTR(".m4v"),
    FSSTR(".mp4"),
    FSSTR(".webm"),

#if defined(WIN32) || defined(_WIN32)
    FSSTR(".wmv"),
#endif
};

std::unordered_set<fs_str_t> getValidExtensions()
{
    std::unordered_set<fs_str_t> result;

    std::copy(imageExtensions.begin(), imageExtensions.end(), std::inserter(result, result.begin()));
    std::copy(animationExtensions.begin(), animationExtensions.end(), std::inserter(result, result.begin()));
    std::copy(videoExtensions.begin(), videoExtensions.end(), std::inserter(result, result.begin()));

    return result;
}

const std::unordered_set<fs_str_t> validExtensions = getValidExtensions();

fs_str_t fsStrToLower(const fs_str_t& src)
{
    return qstringToFsstr(fsstrToQstring(src).toLower());
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

    loadLinks();

    srand(std::chrono::system_clock::now().time_since_epoch().count());

    player = std::make_unique<QMediaPlayer>();
    playlist = std::make_unique<QMediaPlaylist>(player.get());
    video = std::make_unique<QVideoWidget>();

    QFont videoInfoFont("Courier New");
    videoInfoFont.setBold(true);
    videoInfoFont.setPixelSize(12);

    videoInfoLabel = std::make_unique<QLabel>(this);
    videoInfoLabel->setFont(videoInfoFont);
    videoInfoLabel->setVisible(false);
    videoInfoLabel->setAutoFillBackground(false);
    videoInfoLabel->setStyleSheet("color: #EEEEEE;");

    videoInfoFontMetrics = std::make_unique<QFontMetrics>(videoInfoLabel->fontMetrics());

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
    videoInfoLabel->setGeometry(0, 0, videoInfoFontMetrics->horizontalAdvance(QString::fromStdString(posText)), videoInfoLabel->font().pixelSize());
}

void MainWindow::hideVideoInfo()
{
    videoInfoLabel->setText("");
}

void MainWindow::togglePauseVideo()
{
    if (!videoMode)
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
    if (!videoMode)
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

void MainWindow::addZoom(float amount)
{
    if (!videoMode)
    {
        zoom += amount;
        if (zoom < 1.0F)
        {
            zoom = 1;
        }
        reloadCurrentImage();
    }
}

void MainWindow::addOffset(float x, float y)
{
    if (!videoMode)
    {
        currentX += x;
        currentY += y;
        reloadCurrentImage();
    }
}

void MainWindow::resetZoomAndOffset()
{
    zoom = 1;
    currentX = 0;
    currentY = 0;
    if (!videoMode)
    {
        reloadCurrentImage();
    };
}

void MainWindow::hideVideo()
{
    videoMode = false;
    video->setVisible(false);
    videoInfoLabel->setVisible(false);
}

void MainWindow::showVideo()
{
    videoMode = true;
    video->setVisible(true);
    videoInfoLabel->setVisible(true);
}

void MainWindow::hideImage()
{
    videoMode = true;
    ui->image_view->setVisible(false);
}

void MainWindow::showImage()
{
    videoMode = false;
    ui->image_view->setVisible(true);
}

void MainWindow::loadLinks()
{
    auto curPath = getExeDir();
    auto linksPath = curPath + DIR_SEPARATOR + LINKS_FILE;
    if (std::filesystem::exists(linksPath))
    {
        std::stringstream sstr;
        std::ifstream ifs(linksPath);

        sstr << ifs.rdbuf();

        std::string text = sstr.str();

        if (text.empty())
        {
            return;
        }

        QString qtext = QString::fromStdString(text);
        for (const auto& line : qtext.replace("\r", "").split('\n'))
        {
            auto segments = line.split(':');
            QChar key = segments[0].at(0);
            QString dir = segments[1];

            links.emplace(key.toLatin1(), qstringToFsstr(dir));
        }
    }
}

void MainWindow::copyToDir(const fs_str_t& dir)
{
    auto showTip = [&]() {
        videoInfoLabel->setVisible(true);
        videoInfoLabel->setText("Copied to " + fsstrToQstring(dir));

        std::thread([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(750));
            if (!videoMode)
            {
                QMetaObject::invokeMethod(this, [&]() { videoInfoLabel->setVisible(false); });
            }
        }).detach();
    };

    auto fulldir = currentDir + dir;
    auto dirUp1 = currentDir + FSSTR("..") + DIR_SEPARATOR + dir;
    if (std::filesystem::exists(fulldir) && std::filesystem::exists(target))
    {        
        auto dest = fulldir + DIR_SEPARATOR + getTargetFilename(target);
        bool copy_ok = std::filesystem::copy_file(target, dest, std::filesystem::copy_options::skip_existing);
        showTip();
    }
    else if (std::filesystem::exists(dirUp1) && std::filesystem::exists(target))
    {
        auto dest = dirUp1 + DIR_SEPARATOR + getTargetFilename(target);
        bool copy_ok = std::filesystem::copy_file(target, dest, std::filesystem::copy_options::skip_existing);
        showTip();
    }    
}

void MainWindow::checkLinksInput(int kkey)
{
    for (const auto& [key, dir] : links)
    {
        if (toupper(kkey) == key || tolower(kkey) == key)
        {
            copyToDir(dir);
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent* e)
{
    QMainWindow::keyPressEvent(e);

    bool altPressed = e->modifiers().testFlag(Qt::KeyboardModifier::AltModifier);
    bool shiftPressed = e->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier);
    bool ctrlPressed = e->modifiers().testFlag(Qt::KeyboardModifier::ControlModifier);
    bool numpadPressed = e->modifiers().testFlag(Qt::KeyboardModifier::KeypadModifier);

    if (ctrlPressed && shiftPressed)
    {
        checkLinksInput(e->key());
    }

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
        loadRandom();
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

    case Qt::Key_Plus:
        addZoom(0.1F);
        break;

    case Qt::Key_Minus:
        addZoom(-0.1F);
        break;

    case Qt::Key_8:
        addOffset(0, 20);
        break;

    case Qt::Key_6:
        addOffset(-20, 0);
        break;

    case Qt::Key_4:
        addOffset(20, 0);
        break;

    case Qt::Key_2:
        addOffset(0, -20);
        break;

    case Qt::Key_0:
        if(altPressed && videoMode)
        {
            resetVideoSpeed();
        }
        else
        {
            resetZoomAndOffset();
        }
        break;        

    default:
        break;
    }
}

template<typename T>
T getSign(T val)
{
    return val < 0 ? -1 : 1;
}

QPixmap MainWindow::getTransformedPixmap(const QPixmap* px)
{
    // TODO: fix zoom-out overflowing
    if (!ui->image_view->pixmap(Qt::ReturnByValue) || ui->image_view->pixmap(Qt::ReturnByValue).isNull())
    {
        return px->scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    float pxmapW = ui->image_view->pixmap(Qt::ReturnByValue).width();
    float pxmapH = ui->image_view->pixmap(Qt::ReturnByValue).height();

    float maxRadiusX = std::abs((pxmapW - width()) / 2);
    float maxRadiusY = std::abs((pxmapH - height()) / 2);

    if (pxmapW <= width())
    {
        currentX = 0;
    }

    if (pxmapH <= height())
    {
        currentY = 0;
    }

    if (std::abs(currentX) > maxRadiusX)
    {
        currentX = maxRadiusX * getSign(currentX);
    }

    if (std::abs(currentY) > maxRadiusY)
    {
        currentY = maxRadiusY * getSign(currentY);
    }

    QPixmap result = *px;
    result.scroll(currentX, currentY, result.rect());
    return result.scaled(size() * zoom, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    if (!videoMode && !ui->image_view->pixmap(Qt::ReturnByValue).isNull())
    {
        QPixmap pxmap = ui->image_view->pixmap(Qt::ReturnByValue);
        ui->image_view->setPixmap(getTransformedPixmap(&pxmap));
        resizeTimer.start(200);
    }
    QWidget::resizeEvent(e);
}

void MainWindow::resizeEnd()
{
    if (!videoMode)
    {
        reloadCurrentImage();
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
    return FSSTR("ffmpeg -y -i \"") + source + FSSTR("\" -pix_fmt yuv420p \"") + target + FSSTR("\"");
}

fs_str_t getCachedAnimatedPath(const fs_str_t& target)
{
    auto cached_path = getTargetDirectory(target) + CACHE_DIR + DIR_SEPARATOR + getTargetFilename(target) + OS_VID_FMT;

    if (!std::filesystem::exists(cached_path))
    {
        initCacheDir(target);

        fs_str_t ffmpeg_cmd_mp4 = genFfmpegCmd(target, cached_path);
        fs_system(ffmpeg_cmd_mp4.c_str());

        bool ok = std::filesystem::exists(target);
        if (!ok)
        {
            return fs_str_t();
        }
    }

    return cached_path;
}

void MainWindow::playVideo(const fs_str_t& vpath)
{
    hideImage();
    showVideo();

    ui->image_view->setPixmap(QPixmap());

    auto media = QUrl::fromLocalFile(fsstrToQstring(vpath.c_str()));

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
        while (videoMode)
        {
            QMetaObject::invokeMethod(this, [&] { showVideoInfo(); });
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        QMetaObject::invokeMethod(this, [&] { hideVideoInfo(); });
    }).detach();
}

void MainWindow::playImage(const fs_str_t& ipath)
{
    hideVideo();
    showImage();

    player->stop();
    playlist->clear();

    currentImage = QImage(fsstrToQstring(ipath));

    QPixmap pxmap = QPixmap::fromImage(*currentImage);
    ui->image_view->setPixmap(getTransformedPixmap(&pxmap));

    return;
}

void MainWindow::playImage(QImage* image)
{
    hideVideo();
    showImage();

    player->stop();
    playlist->clear();

    currentImage = *image;

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
    setWindowTitle(fsstrToQstring(getTargetFilename(target)));

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
    if (itemListIndex != 0)
    {
        std::thread([&, idx = itemListIndex]()
        {
            std::lock_guard lock(surroundingPrevMux);
            surroundingPrevReady = false;
            const auto& prevTarget = itemList[idx - 1];
            prevName = prevTarget;

            if (isImage(prevTarget))
            {
                surroundingPrev = QImage(fsstrToQstring(prevTarget));
                surroundingPrevReady = true;
            }
        }).detach();
    }
}

void MainWindow::loadSurroundingNext()
{
    if (itemListIndex < itemList.size() - 1)
    {
        std::thread([&, idx = itemListIndex]()
        {
            std::lock_guard lock(surroundingNextMux);
            surroundingNextReady = false;
            const auto& nextTarget = itemList[idx + 1];
            nextName = nextTarget;

            if (isImage(nextTarget))
            {
                surroundingNext = QImage(fsstrToQstring(nextTarget));
                surroundingNextReady = true;
            }
        }).detach();
    }
}

void MainWindow::previousItem()
{
    resetZoomAndOffset();
    if (!itemListReady || itemListIndex == 0)
    {
        return;
    }

    --itemListIndex;
    if (surroundingPrevReady && surroundingPrev && !surroundingPrev.value().isNull())
    {
        if (!videoMode)
        {
            surroundingNext = currentImage;
            nextName = target;
        }
        else
        {
            surroundingNext = std::nullopt;
            nextName = FSSTR("");
        }

        target = prevName;
        setWindowTitle(fsstrToQstring(getTargetFilename(prevName)));
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
    resetZoomAndOffset();
    if (!itemListReady || (itemListIndex) == itemList.size() - 1)
    {
        return;
    }
    ++itemListIndex;
    if (surroundingNextReady && surroundingNext.has_value() && !surroundingNext.value().isNull())
    {
        if (!videoMode)
        {
            surroundingPrev = currentImage;
            prevName = target;
        }
        else
        {
            surroundingPrev = std::nullopt;
            prevName = FSSTR("");
        }

        target = nextName;
        setWindowTitle(fsstrToQstring(getTargetFilename(nextName)));
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
    resetZoomAndOffset();
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
    resetZoomAndOffset();
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

void MainWindow::reloadCurrentImage()
{
    playImage(&currentImage.value());
}

void MainWindow::setupItemList()
{
    namespace stdfs = std::filesystem;
    std::multimap<long long, stdfs::path> paths;
    for (const auto& f : stdfs::directory_iterator(currentDir))
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
