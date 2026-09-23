#include "battle_screen.h"
#include "ai/engine/ai_levels.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QGraphicsDropShadowEffect>

void BattleScreen::handleLevelModeEnd(const LevelResult& lr)
{
    m_gameOver = true;
    disableActionButtons();

    bool playerDead = m_campaign.isGameOver();
    bool allCleared = m_campaign.isVictory();

    QDialog dlg(this);
    dlg.setMinimumSize(420, 420);
    dlg.setStyleSheet(R"(
        QDialog { background-color: #1e1e2a; }
        QLabel { color: #FFFFFF; font-size: 16px; }
        QPushButton {
            background-color: #E8503A;
            color: #FFFFFF;
            border: 2px solid #FFFFFF;
            border-radius: 8px;
            padding: 10px 24px;
            font-size: 15px;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #F0634E; }
    )");

    QGraphicsDropShadowEffect* dlgShadow = new QGraphicsDropShadowEffect(&dlg);
    dlgShadow->setBlurRadius(24);
    dlgShadow->setOffset(0, 10);
    dlgShadow->setColor(QColor(0, 0, 0, 180));
    dlg.setGraphicsEffect(dlgShadow);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    // --- 标题 ---
    QString headerText;
    QString headerColor;
    if (allCleared) {
        headerText = "全部通关！";
        headerColor = "#FFD700";
    } else if (playerDead) {
        headerText = "生命耗尽！";
        headerColor = "#EF5350";
    } else if (lr.playerWon) {
        headerText = "恭喜胜利！";
        headerColor = "#FFD700";
    } else {
        headerText = "挑战失败";
        headerColor = "#EF5350";
    }
    auto* header = new QLabel(headerText);
    QFont hf = header->font();
    hf.setPointSize(22);
    hf.setBold(true);
    header->setFont(hf);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(QString("QLabel { color: %1; }").arg(headerColor));
    layout->addWidget(header);

    // --- 关卡信息 ---
    auto* status = new QLabel(QString("关卡模式 - 第 %1 关").arg(m_currentLevel));
    status->setAlignment(Qt::AlignCenter);
    status->setStyleSheet("QLabel { color: #B0BEC5; font-size: 14px; }");
    layout->addWidget(status);

    // --- 分数 ---
    auto* scoreInfo = new QLabel(QString("玩家A 得分: %1 分 | 电脑 得分: %2 分")
        .arg(lr.playerScore)
        .arg(lr.bossScore));
    scoreInfo->setAlignment(Qt::AlignCenter);
    layout->addWidget(scoreInfo);

    // --- HP 变化 ---
    QString hpDeltaStr = QString("Boss 血量: %1 %2  |  玩家血量: %3 %4")
        .arg(m_campaign.bossHp() - lr.bossHpDelta)
        .arg(lr.bossHpDelta <= 0 ? QString("%1").arg(lr.bossHpDelta) : QString("+%1").arg(lr.bossHpDelta))
        .arg(m_campaign.playerHp() - lr.playerHpDelta)
        .arg(lr.playerHpDelta >= 0 ? QString("+%1").arg(lr.playerHpDelta) : QString("%1").arg(lr.playerHpDelta));

    auto* hpInfo = new QLabel(QString("血量: 玩家 %1/%2  |  Boss %3/110")
        .arg(m_campaign.playerHp())
        .arg(m_campaign.config().playerMaxHp)
        .arg(m_campaign.bossHp()));
    hpInfo->setAlignment(Qt::AlignCenter);
    hpInfo->setStyleSheet("QLabel { color: #FFAB40; font-size: 15px; font-weight: bold; }");
    layout->addWidget(hpInfo);

    // --- 剩余牌 ---
    auto* remainInfo = new QLabel(QString("玩家剩余牌: %1 张 | Boss剩余牌: %2 张")
        .arg(lr.playerRemainCards)
        .arg(lr.bossRemainCards));
    remainInfo->setAlignment(Qt::AlignCenter);
    remainInfo->setStyleSheet("QLabel { color: #90CAF9; font-size: 14px; }");
    layout->addWidget(remainInfo);

    layout->addStretch();

    // --- 通关 ---
    if (allCleared) {
        auto* finalTitle = new QLabel("全部通关！");
        QFont ft = finalTitle->font();
        ft.setPointSize(18);
        ft.setBold(true);
        finalTitle->setFont(ft);
        finalTitle->setAlignment(Qt::AlignCenter);
        finalTitle->setStyleSheet("QLabel { color: #FFD700; }");
        layout->addWidget(finalTitle);

        auto* finalMsg = new QLabel("恭喜你击败了所有9个AI对手！");
        finalMsg->setAlignment(Qt::AlignCenter);
        finalMsg->setStyleSheet("QLabel { color: #B0BEC5; font-size: 15px; }");
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

    // --- 玩家死亡 ---
    if (playerDead) {
        auto* deathMsg = new QLabel("你的生命值已归零，游戏结束。");
        deathMsg->setAlignment(Qt::AlignCenter);
        deathMsg->setStyleSheet("QLabel { color: #EF9A9A; font-size: 15px; }");
        layout->addWidget(deathMsg);

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

    // --- 玩家胜利（未通关）---
    if (lr.playerWon) {
        auto* nextMsg = new QLabel(QString("准备挑战第 %1 关").arg(m_currentLevel + 1));
        nextMsg->setAlignment(Qt::AlignCenter);
        nextMsg->setStyleSheet("QLabel { color: #B0BEC5; font-size: 15px; }");
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
        // --- 玩家失败 ---
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