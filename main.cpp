#include <QApplication>
#include <QLoggingCategory>
#include "ui/mainwindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
    // 屏蔽 Qt Multimedia FFmpeg 日志
    QLoggingCategory::setFilterRules(
        "qt.multimedia.ffmpeg=false\n"
        "qt.multimedia.*=false\n"
    );

#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("7鬼523斗地主变体");

    MainWindow window;
    window.show();

    return app.exec();
}