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
* `Alt+Left/Right arrow`: Increase/decrease playback speed
* `Alt+0`: Reset playback speed

## **Build requirements**

* CMake >= 3.14
* C++17 compatible compiler
* Qt5
* Set ${QT_DIR} to Qt SDK path (example on Windows: C:\Qt\5.15.2\msvc2019_64)

## **Usage requirements**

* `ffmpeg` in PATH
* Appropiate video drivers for input formats
