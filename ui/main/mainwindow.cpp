#include "mainwindow.h"
#include "ui/screens/battle_screen.h"

#include <QResizeEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("7鬼523斗地主变体");
    setMinimumSize(1200, 800);
    resize(1200, 800);

    setStyleSheet(R"(
        QMainWindow {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1B5E20, stop:0.5 #2E7D32, stop:1 #1B5E20);
        }
    )");

    // 屏幕栈
    m_screenStack = new QStackedWidget(this);
    setCentralWidget(m_screenStack);

    // 路由器
    m_router = new ScreenRouter(m_screenStack, this);

    // 注册 BattleScreen
    m_battleScreen = new BattleScreen(m_screenStack);
    m_router->registerScreen(ScreenId::Battle, m_battleScreen);

    // 默认显示 BattleScreen
    m_router->switchTo(ScreenId::Battle);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}