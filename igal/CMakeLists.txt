cmake_minimum_required(VERSION 3.13)

if(WIN32)
    set(CMAKE_PREFIX_PATH $ENV{QT_DIR})
endif()

find_package(
    Qt5
    REQUIRED
    COMPONENTS Core Gui Multimedia MultimediaWidgets Widgets
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    add_executable(igal WIN32 main.cpp)
else()
    add_executable(igal main.cpp)
endif()

function(ADD_WIDGET NAME)
    target_sources(igal PRIVATE
        ${NAME}.cpp
        ${NAME}.h
        ${NAME}.ui
    )
endfunction()

ADD_WIDGET(mainwindow)

target_link_libraries(igal
    Qt5::Core
    Qt5::Gui
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    Qt5::Widgets
)

if(WIN32)
    target_sources(igal PRIVATE win32/resources.rc win32/utils.cpp)

    set(QT_WINDEPLOY_PATH $ENV{QT_DIR}/bin/windeployqt.exe)

    add_custom_command(
        TARGET igal
        COMMAND ${QT_WINDEPLOY_PATH} $<TARGET_FILE:igal> 
        --no-translations 
        --no-opengl-sw 
        --no-angle
        --no-system-d3d-compiler
        --no-compiler-runtime
    )
else()
    target_sources(igal PRIVATE posix/utils.cpp)

    find_package(Threads REQUIRED)
    target_link_libraries(igal Threads::Threads)
endif()