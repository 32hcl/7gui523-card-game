#include "battle_screen.h"
#include "ai/engine/ai_levels.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QGraphicsDropShadowEffect>

void BattleScreen::handleLevelModeEnd(bool playerWon)
{
    m_gameOver = true;
    disableActionButtons();

    QDialog dlg(this);
    dlg.setMinimumSize(420, 360);
    dlg.setStyleSheet(R"(
        QDialog { background-color: #1B5E20; }
        QLabel { color: #FFFFFF; font-size: 16px; }
        QPushButton {
            background-color: #2E7D32;
            color: #FFFFFF;
            border: 2px solid #66BB6A;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 15px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #388E3C; }
    )");

    QGraphicsDropShadowEffect* dlgShadow = new QGraphicsDropShadowEffect(&dlg);
    dlgShadow->setBlurRadius(24);
    dlgShadow->setOffset(0, 10);
    dlgShadow->setColor(QColor(0, 0, 0, 180));
    dlg.setGraphicsEffect(dlgShadow);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto* header = new QLabel(playerWon ? "恭喜胜利！" : "挑战失败");
    QFont hf = header->font();
    hf.setPointSize(22);
    hf.setBold(true);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(playerWon
        ? "QLabel { color: #FFD700; }"
        : "QLabel { color: #EF5350; }");
    layout->addWidget(header);

    auto* status = new QLabel(QString("关卡模式 - 第 %1 关").arg(m_currentLevel));
    status->setAlignment(Qt::AlignCenter);
    status->setStyleSheet("QLabel { color: #B0BEC5; font-size: 14px; }");
    layout->addWidget(status);

    auto* scoreInfo = new QLabel(QString("玩家A: %1 分 | 电脑: %2 分")
        .arg(m_playerA.totalScore)
        .arg(m_playerB.totalScore));
    scoreInfo->setAlignment(Qt::AlignCenter);
    layout->addWidget(scoreInfo);

    layout->addStretch();

    if (playerWon && m_currentLevel >= 13) {
        auto* finalTitle = new QLabel("全部通关！");
        QFont ft = finalTitle->font();
        ft.setPointSize(18);
        ft.setBold(true);
        finalTitle->setFont(ft);
        finalTitle->setAlignment(Qt::AlignCenter);
        finalTitle->setStyleSheet("QLabel { color: #FFD700; }");
        layout->addWidget(finalTitle);

        auto* finalMsg = new QLabel("恭喜你击败了所有13个AI对手！");
        finalMsg->setAlignment(Qt::AlignCenter);
        finalMsg->setStyleSheet("QLabel { color: #A5D6A7; font-size: 15px; }");
        layout->addWidget(finalMsg);

        layout->addStretch();

        auto* btnLayout = new QHBoxLayout;
        auto* menuBtn = new QPushButton("返回主菜单");
        btnLayout->addStretch();
        btnLayout->addWidget(menuBtn);
        btnLayout->addStretch();
        layout->addLayout(btnLayout);

        connect(menuBtn, &QPushButton::clicked, this, &BattleScreen::returnToMenu);
        connect(menuBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

        dlg.exec();
        return;
    }

    if (playerWon) {
        auto* nextMsg = new QLabel(QString("准备挑战第 %1 关").arg(m_currentLevel + 1));
        nextMsg->setAlignment(Qt::AlignCenter);
        nextMsg->setStyleSheet("QLabel { color: #A5D6A7; font-size: 15px; }");
        layout->addWidget(nextMsg);

        layout->addStretch();

        auto* btnLayout = new QHBoxLayout;
        auto* nextBtn = new QPushButton("下一关");
        auto* exitBtn = new QPushButton("退出");
        btnLayout->addWidget(nextBtn);
        btnLayout->addWidget(exitBtn);
        layout->addLayout(btnLayout);

        connect(nextBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(exitBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        int ret = dlg.exec();
        if (ret == QDialog::Accepted) {
            m_currentLevel++;
            m_gameOver = false;
            onNewGameButtonClicked();
        } else {
            returnToMenu();
        }
    } else {
        auto* failMsg = new QLabel(
            QString("你输给了: %1").arg(getLevelDisplayName(m_currentLevel)));
        failMsg->setAlignment(Qt::AlignCenter);
        failMsg->setStyleSheet("QLabel { color: #EF9A9A; font-size: 15px; }");
        layout->addWidget(failMsg);

        layout->addStretch();

        auto* btnLayout = new QHBoxLayout;
        auto* retryBtn = new QPushButton("重试本关");
        auto* exitBtn = new QPushButton("退出");
        btnLayout->addWidget(retryBtn);
        btnLayout->addWidget(exitBtn);
        layout->addLayout(btnLayout);

        connect(retryBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(exitBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        int ret = dlg.exec();
        if (ret == QDialog::Accepted) {
            m_gameOver = false;
            onNewGameButtonClicked();
        } else {
            returnToMenu();
        }
    }
}

void BattleScreen::returnToMenu()
{
    emit gameEnded();
}