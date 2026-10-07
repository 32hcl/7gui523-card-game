#include "mainwindow.h"
#include "ui/screens/battle_screen.h"
#include "ui/screens/main_menu_screen.h"
#include "ui/screens/level_select_screen.h"
#include "ui/theme/theme_manager.h"
#include "ui/audio/audio_manager.h"
#include "ui/audio/audio_generator.h"

#include <QResizeEvent>
#include <QApplication>
#include <QDir>
#include <QCoreApplication>
#include <QFile>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("7鬼523斗地主变体");
    setMinimumSize(1200, 800);
    resize(1200, 800);

    // ── 应用主题 ──
    ThemeManager::instance()->applyTheme(qApp, ThemeManager::Theme::Casino);

    // ── 初始化音频系统 ──
    QString audioDir = QCoreApplication::applicationDirPath() + "/audio/";
    QDir().mkpath(audioDir);
    // 首次启动时生成所有音效文件
    QString testFile = audioDir + "bgm_menu.wav";
    if (!QFile::exists(testFile)) {
        AudioGenerator::generateAll(audioDir);
    }
    AudioManager::instance().init(audioDir);

    setStyleSheet(R"(
        QMainWindow {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a1a1a, stop:0.5 #3a2020, stop:1 #2a1a1a);
        }
    )");

    // 屏幕栈
    m_screenStack = new QStackedWidget(this);
    setCentralWidget(m_screenStack);

    // 路由器
    m_router = new ScreenRouter(m_screenStack, this);

    // 注册 MainMenuScreen
    m_menuScreen = new MainMenuScreen(m_screenStack);
    m_router->registerScreen(ScreenId::MainMenu, m_menuScreen);
    connect(m_menuScreen, &MainMenuScreen::practiceModeSelected, this, &MainWindow::onPracticeMode);
    connect(m_menuScreen, &MainMenuScreen::levelModeSelected, this, &MainWindow::onLevelMode);

    // 注册 BattleScreen
    m_battleScreen = new BattleScreen(m_screenStack);
    m_router->registerScreen(ScreenId::Battle, m_battleScreen);
    connect(m_battleScreen, &BattleScreen::gameEnded, this, &MainWindow::onBattleGameEnded);

    // 注册 LevelSelectScreen
    m_levelSelectScreen = new LevelSelectScreen(m_screenStack);
    m_router->registerScreen(ScreenId::LevelSelect, m_levelSelectScreen);
    connect(m_levelSelectScreen, &LevelSelectScreen::backToMenu, this, &MainWindow::onLevelSelectBack);
    connect(m_levelSelectScreen, &LevelSelectScreen::levelSelected, this, &MainWindow::onLevelSelected);

    // 默认显示主菜单
    m_router->switchTo(ScreenId::MainMenu);
}

void MainWindow::onPracticeMode()
{
    m_battleScreen->setLevelMode(false, 1);
    m_router->switchTo(ScreenId::Battle);
    m_battleScreen->startNewGame();
}

void MainWindow::onLevelMode()
{
    // 进入关卡选择界面
    m_levelSelectScreen->setProgress(0);  // TODO: 从存档读取进度
    m_levelSelectScreen->setChips(100);   // TODO: 从存档读取筹码
    m_router->switchTo(ScreenId::LevelSelect);
}

void MainWindow::onLevelSelected(int level)
{
    m_battleScreen->setLevelMode(true, level);
    m_router->switchTo(ScreenId::Battle);
    m_battleScreen->startNewGame();
}

void MainWindow::onLevelSelectBack()
{
    m_router->switchTo(ScreenId::MainMenu);
}

void MainWindow::onBattleGameEnded()
{
    // 战斗结束后回到关卡选择（如果是关卡模式）
    // TODO: 根据胜负更新进度
    m_router->switchTo(ScreenId::LevelSelect);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}