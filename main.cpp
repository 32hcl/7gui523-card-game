#include <QApplication>
#include <QLoggingCategory>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include "ui/main/mainwindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

static QFile* g_logFile = nullptr;
static void messageHandler(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    if (!g_logFile) {
        g_logFile = new QFile(QCoreApplication::applicationDirPath() + "/debug.log");
        g_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
    }
    QTextStream(g_logFile) << msg << "\n";
    g_logFile->flush();
}

int main(int argc, char* argv[])
{
    qInstallMessageHandler(messageHandler);
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