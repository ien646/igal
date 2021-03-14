#include <QApplication>

#include <iostream>

#include "defs.h"
#include "mainwindow.h"

#ifdef WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    int pargc = argc;
    QApplication app(argc, nullptr);
    argc = pargc;

    if (argc < 2)
    {
        std::cerr << "No target argument provided!\n";
        return 1;
    }

    fs_str_t target(argv[1]);

    MainWindow win(target);
    win.show();

    return app.exec();
}