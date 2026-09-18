#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "ui/router/screen_router.h"

class BattleScreen;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QStackedWidget* m_screenStack;
    ScreenRouter*   m_router;
    BattleScreen*   m_battleScreen;
};