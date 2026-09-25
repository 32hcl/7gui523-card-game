#include <QApplication>
#include <QLoggingCategory>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QTimer>
#include "ui/main/mainwindow.h"
#include "ui/screens/battle_screen.h"

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

    if (argc >= 3 && QString(argv[1]) == "--stress-test") {
        int n = QString(argv[2]).toInt();
        if (n <= 0) n = 100;

        BattleScreen* screen = new BattleScreen();
        screen->show();

        QTimer::singleShot(2000, screen, [screen, n]() {
            screen->runAutoPlayTest(n);
        });

        QObject::connect(screen, &BattleScreen::autoPlayTestFinished,
                         &app, &QApplication::quit);

        return app.exec();
    }

    MainWindow window;
    window.show();

    return app.exec();
}