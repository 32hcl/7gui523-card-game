#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "ui/router/screen_router.h"

class BattleScreen;
class MainMenuScreen;
class LevelSelectScreen;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onPracticeMode();
    void onLevelMode();
    void onLevelSelected(int level);
    void onLevelSelectBack();
    void onBattleGameEnded();

private:
    QStackedWidget*    m_screenStack;
    ScreenRouter*      m_router;
    BattleScreen*      m_battleScreen;
    MainMenuScreen*    m_menuScreen;
    LevelSelectScreen* m_levelSelectScreen;
};