
# igal: Multimedia gallery

## **Controls**

### Common:

* `Left/Right arrow`: Next image/video
* `F`: Toggle fullscreen
* `Escape (while in fullscreen)`: Disable fullscreen
* `R`: Go to random item in current directory

### In image-mode:

* `Plus "+" sign`: Increase zoom
* `Minus "-" sign`: Decrease zoom
* `2, 4, 6, 8 (numpad arrows)`: Move image
* `0`: Reset zoom/offset

### In video-mode:

* `Ctrl+Left/Rigth arrow`: Skip/rewind video
* `Ctrl+Shift+Left/Right arrow`: Skip/rewind video (fast)
* `Shift+Up/Down arrow`: Increase/decrease playback speed
* `Alt+0`: Reset playback speed
* `M`: Mute audio
* `P`: Stop/Resume

## **Build requirements**

* CMake >= 3.14
* C++17 compatible compiler
* Qt5 and Qt5Multimedia extensions. 
* <u>**Windows specific**</u>:
    * Set ${QT_DIR} to Qt SDK path (example: C:\Qt\5.15.2\msvc2019_64)
* <u>**Linux specific**</u>:
	* libqt5multimedia5-plugins (video playback)

## **Usage requirements**
* `ffmpeg` available in $PATH
* Appropiate video drivers for playback


