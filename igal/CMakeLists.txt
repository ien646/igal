cmake_minimum_required(VERSION 3.13)

set(CMAKE_PREFIX_PATH $ENV{QT_DIR})

find_package(
    Qt5
    REQUIRED
    COMPONENTS Core Gui Multimedia MultimediaWidgets Widgets
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(igal
    main.cpp
)

function(ADD_WIDGET NAME)
    target_sources(igal PRIVATE
        ${NAME}.cpp
        ${NAME}.h
        ${NAME}.ui
    )
endfunction()

ADD_WIDGET(mainwindow)

target_include_directories(igal PRIVATE ${CMAKE_SOURCE_DIR}/libpng)

target_link_libraries(igal
    Qt5::Core
    Qt5::Gui
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    Qt5::Widgets
)

if(WIN32)
    set(QT_WINDEPLOY_PATH $ENV{QT_DIR}/bin/windeployqt.exe)

    add_custom_command(
        TARGET igal
        COMMAND ${QT_WINDEPLOY_PATH} $<TARGET_FILE:igal>
    )
endif()

