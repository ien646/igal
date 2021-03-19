#include <QtWidgets/qapplication.h>

#include <iostream>

#include "defs.h"
#include "mainwindow.h"

int mainBody(int argc, const fs_str_t& target);

#if defined(WIN32) || defined(_WIN32)

#include <Windows.h>

int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     PWSTR     lpCmdLine,
    _In_     int       nCmdShow)
{
    int argc;
    LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);

    if (argc < 1)
    {
        std::cerr << "No target argument provided!\n";
        return 1;
    }

    fs_str_t target(argv[0]);
    return mainBody(argc, target);
}

#else

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "No target argument provided!\n";
        return 1;
    }

    fs_str_t target(argv[1]);
    return mainBody(argc, target);
}

#endif

int mainBody(int argc, const fs_str_t& target)
{
    QApplication app(argc, nullptr);

    MainWindow win(target);
    win.show();

    return app.exec();
}