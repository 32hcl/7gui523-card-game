#include "main_menu_screen.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QFont>

MainMenuScreen::MainMenuScreen(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(R"(
        QWidget {
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }
        QPushButton {
            background-color: #E8503A;
            color: #FFFFFF;
            border: 2px solid #FFFFFF;
            border-radius: 14px;
            padding: 20px 40px;
            font-size: 22px;
            font-weight: bold;
            min-width: 300px;
        }
        QPushButton:hover {
            background-color: #F0634E;
        }
        QPushButton:pressed {
            background-color: #C43D28;
        }
        QLabel#descLabel {
            color: #B0BEC5;
            font-size: 14px;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(30);
    layout->setAlignment(Qt::AlignCenter);

    // Title
    auto* title = new QLabel("7鬼523斗地主变体");
    QFont titleFont = title->font();
    titleFont.setPointSize(36);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("QLabel { color: #FFD700; }");
    layout->addWidget(title);

    // Subtitle
    auto* subtitle = new QLabel("选择游戏模式");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("QLabel { color: #F5A623; font-size: 18px; }");
    layout->addWidget(subtitle);

    layout->addSpacing(40);

    // Practice Mode Button
    auto* practiceBtn = new QPushButton("练习模式");
    {
        QGraphicsDropShadowEffect* s = new QGraphicsDropShadowEffect(practiceBtn);
        s->setBlurRadius(16);
        s->setOffset(0, 4);
        s->setColor(QColor(0, 0, 0, 140));
        practiceBtn->setGraphicsEffect(s);
    }
    auto* practiceDesc = new QLabel("自由选择对手难度与先后手");
    practiceDesc->setObjectName("descLabel");
    practiceDesc->setAlignment(Qt::AlignCenter);
    practiceDesc->setStyleSheet("QLabel#descLabel { color: #B0BEC5; font-size: 14px; }");

    layout->addWidget(practiceBtn, 0, Qt::AlignCenter);
    layout->addWidget(practiceDesc, 0, Qt::AlignCenter);

    layout->addSpacing(20);

    // Level Mode Button
    auto* levelBtn = new QPushButton("关卡模式");
    {
        QGraphicsDropShadowEffect* s = new QGraphicsDropShadowEffect(levelBtn);
        s->setBlurRadius(16);
        s->setOffset(0, 4);
        s->setColor(QColor(0, 0, 0, 140));
        levelBtn->setGraphicsEffect(s);
    }
    auto* levelDesc = new QLabel("依次挑战 9 个电脑，全部通关才算赢");
    levelDesc->setObjectName("descLabel");
    levelDesc->setAlignment(Qt::AlignCenter);

    layout->addWidget(levelBtn, 0, Qt::AlignCenter);
    layout->addWidget(levelDesc, 0, Qt::AlignCenter);

    connect(practiceBtn, &QPushButton::clicked, this, &MainMenuScreen::practiceModeSelected);
    connect(levelBtn, &QPushButton::clicked, this, &MainMenuScreen::levelModeSelected);
}