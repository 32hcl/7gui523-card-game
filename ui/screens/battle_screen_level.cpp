#include "battle_screen.h"
#include "ai/ai_levels.h"
#include "ui/dialogs/shop_dialog.h"
#include "ui/widgets/dialogue_overlay.h"
#include "dialogue/dialogue_engine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialog>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMessageBox>

namespace {
    const QColor kDialogShadowColor(0, 0, 0, 180);
}

void BattleScreen::handleLevelModeEnd(const LevelResult& lr)
{
    m_phase = GamePhase::GameOver;
    disableActionButtons();

    // 暂存结算结果
    m_pendingLevelResult = lr;

    // 播放胜负对话
    bool playerDead = m_campaign.isGameOver();
    bool allCleared = m_campaign.isVictory();

    if (m_dialogueOverlay && m_isLevelMode) {
        DialogueTrigger trigger = lr.playerWon ? DialogueTrigger::OnPlayerWin : DialogueTrigger::OnBossWin;
        auto lines = DialogueEngine::instance().getDialogue(m_currentLevel, trigger);

        if (!lines.isEmpty()) {
            m_dialogueCtx = lr.playerWon ? DialogueCtx::Victory : DialogueCtx::Defeat;
            m_dialogueOverlay->playDialogue(lines, m_currentLevel);
            return;  // 对话结束后由 onDialogueFinished 调用 showLevelEndDialog()
        }
    }

    // 无对话，直接显示结算
    showLevelEndDialog();
}

void BattleScreen::showLevelEndDialog()
{
    const LevelResult& lr = m_pendingLevelResult;

    bool playerDead = m_campaign.isGameOver();
    bool allCleared = m_campaign.isVictory();

    QDialog dlg(this);
    dlg.setMinimumSize(420, 420);
    dlg.setStyleSheet(R"(
        QDialog {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a1a14, stop:1 #1a0f0a);
            border: 2px solid #8b6508;
            border-radius: 12px;
        }
        QLabel { color: #fff8e7; font-size: 16px; }
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e63946, stop:0.5 #c41e3a, stop:1 #9b1521);
            color: #fff8e7;
            border: 2px solid #ffd700;
            border-radius: 10px;
            padding: 10px 24px;
            font-size: 15px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f04a57, stop:0.5 #d6273f, stop:1 #a81824);
            border: 2px solid #ffe66d;
        }
    )");

    QGraphicsDropShadowEffect* dlgShadow = new QGraphicsDropShadowEffect(&dlg);
    dlgShadow->setBlurRadius(24);
    dlgShadow->setOffset(0, 10);
    dlgShadow->setColor(kDialogShadowColor);
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

    // --- HP ---
    auto* hpInfo = new QLabel(QString("血量: 玩家 %1/%2  |  Boss %3/%4")
        .arg(m_campaign.playerHp())
        .arg(m_campaign.config().playerMaxHp)
        .arg(m_campaign.bossHp())
        .arg(m_campaign.config().bossMaxHp));
    hpInfo->setAlignment(Qt::AlignCenter);
    hpInfo->setStyleSheet("QLabel { color: #FFAB40; font-size: 15px; font-weight: bold; }");
    layout->addWidget(hpInfo);

    // --- 筹码结算 ---
    QString chipText;
    if (lr.chipsDelta > 0) {
        chipText = QString("🎡 奖励筹码: +%1  |  余额: %2").arg(lr.chipsDelta).arg(lr.chipsAfter);
    } else if (lr.chipsDelta < 0) {
        chipText = QString("🎡 损失筹码: %1  |  余额: %2").arg(lr.chipsDelta).arg(lr.chipsAfter);
    } else {
        chipText = QString("🎡 筹码余额: %1").arg(lr.chipsAfter);
    }
    auto* chipsInfo = new QLabel(chipText);
    chipsInfo->setAlignment(Qt::AlignCenter);
    chipsInfo->setStyleSheet("QLabel { color: #ffd700; font-size: 14px; font-weight: bold; }");
    layout->addWidget(chipsInfo);

    // 更新底部筹码显示
    if (m_chipsLabel) {
        m_chipsLabel->setText(QString("🎡 筹码: %1").arg(lr.chipsAfter));
    }

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
        auto* nextBtn = new QPushButton("进入商店");
        auto* exitBtn = new QPushButton("退出");
        btnLayout->addWidget(nextBtn);
        btnLayout->addWidget(exitBtn);
        layout->addLayout(btnLayout);

        connect(nextBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(exitBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

        int ret = dlg.exec();
        if (ret == QDialog::Accepted) {
            // 打开商店
            showShopDialog();

            m_currentLevel++;
            m_phase = GamePhase::DealAnimation;
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
            m_phase = GamePhase::DealAnimation;
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

// 每关结束后打开商店：售卖回血药水和特殊卡牌
void BattleScreen::showShopDialog(bool viewOnly)
{
    ShopDialog dlg(m_campaign.playerChips(), m_campaign.playerHp(),
                   m_campaign.config().playerMaxHp, viewOnly, this);
    dlg.exec();

    // 浏览模式不应用购买结果
    if (viewOnly) return;

    // 应用购买结果
    int chipsSpent = dlg.chipsSpent();
    int hpHealed   = dlg.hpHealed();
    auto purchased = dlg.purchasedCards();
    auto dests     = dlg.purchasedDests();

    if (chipsSpent > 0) {
        m_campaign.spendChips(chipsSpent);
    }
    if (hpHealed > 0) {
        m_campaign.healPlayer(hpHealed);
        appendLog(QString("商店: 回复 %1 HP").arg(hpHealed));
    }
    if (!purchased.empty()) {
        for (size_t i = 0; i < purchased.size(); ++i) {
            const auto& c = purchased[i];
            CardDestination d = (i < dests.size()) ? dests[i] : CardDestination::ToPlayDeck;
            QString destStr;
            switch (d) {
                case CardDestination::ToPlayDeck:
                    m_shopPurchasedCards.push_back(c);
                    destStr = "洗入游玩牌堆";
                    break;
                case CardDestination::ToHiddenDeck:
                    m_playerHiddenDeck.cards.push_back(c);
                    destStr = "进入隐藏牌堆";
                    break;
                case CardDestination::ToHand:
                    m_shopHandCards.push_back(c);
                    destStr = "进入下关手牌";
                    break;
            }
            appendLog(QString("商店: 购买 %1%2 (%3)")
                .arg(QString::fromStdString(c.suit),
                     QString::fromStdString(c.point),
                     destStr));
        }
    }
}

bool BattleScreen::event(QEvent* e)
{
    if (e->type() == QEvent::KeyPress && m_isLevelMode) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Tab) {
            showDebugPanel();
            return true;
        }
    }
    return QWidget::event(e);
}

void BattleScreen::showDebugPanel()
{
    QDialog dlg(this);
    dlg.setWindowTitle("调试面板 - 快速测试");
    dlg.setMinimumSize(340, 340);
    dlg.setStyleSheet(R"(
        QDialog { background-color: #1a1a2e; }
        QLabel { color: #FFFFFF; font-size: 16px; }
        QPushButton {
            background-color: #2a2a4a;
            color: #FFFFFF;
            border: 1px solid #5a5a8a;
            border-radius: 6px;
            padding: 10px 16px;
            font-size: 14px;
        }
        QPushButton:hover { background-color: #3a3a6a; }
    )");

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(8);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* title = new QLabel(QString("调试面板 - 第 %1 关").arg(m_currentLevel));
    QFont tf = title->font();
    tf.setPointSize(15);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* hpLabel = new QLabel(QString("HP: %1/%2 | 筹码: %3")
        .arg(m_campaign.playerHp())
        .arg(m_campaign.config().playerMaxHp)
        .arg(m_campaign.playerChips()));
    hpLabel->setAlignment(Qt::AlignCenter);
    hpLabel->setStyleSheet("QLabel { color: #B0BEC5; }");
    layout->addWidget(hpLabel);

    layout->addSpacing(10);

    // --- 按钮1: 直接胜利跳关 ---
    QPushButton* skipBtn = new QPushButton("跳过当前关卡 (直接胜利)");
    layout->addWidget(skipBtn);
    connect(skipBtn, &QPushButton::clicked, [&]() {
        dlg.accept();
        int nextLevel = m_currentLevel + 1;
        if (nextLevel > m_campaign.config().totalLevels) {
            QMessageBox::information(this, "调试", "已经是最后一关了！");
            return;
        }
        m_currentLevel = nextLevel;
        m_campaign.startLevel(m_currentLevel);
        m_phase = GamePhase::DealAnimation;
        onNewGameButtonClicked();
        appendLog(QString("[调试] 跳过至第 %1 关").arg(m_currentLevel));
    });

    // --- 按钮2: 跳转到指定关卡 ---
    QPushButton* jumpBtn = new QPushButton("跳转到指定关卡");
    layout->addWidget(jumpBtn);
    connect(jumpBtn, &QPushButton::clicked, [&]() {
        dlg.accept();
        bool ok = false;
        int target = QInputDialog::getInt(this, "跳关",
            QString("目标关卡 (1-%1):").arg(m_campaign.config().totalLevels),
            m_currentLevel, 1, m_campaign.config().totalLevels, 1, &ok);
        if (!ok) return;
        m_currentLevel = target;
        m_campaign.startLevel(m_currentLevel);
        m_phase = GamePhase::DealAnimation;
        onNewGameButtonClicked();
        appendLog(QString("[调试] 跳转至第 %1 关").arg(m_currentLevel));
    });

    // --- 按钮3: 进入商店 ---
    QPushButton* shopBtn = new QPushButton("进入商店 (可购买)");
    layout->addWidget(shopBtn);
    connect(shopBtn, &QPushButton::clicked, [&]() {
        dlg.accept();
        showShopDialog(false);
    });

    // --- 按钮4: 回复满血 ---
    QPushButton* healBtn = new QPushButton("回复满血");
    layout->addWidget(healBtn);
    connect(healBtn, &QPushButton::clicked, [&]() {
        dlg.accept();
        int healAmount = m_campaign.config().playerMaxHp - m_campaign.playerHp();
        m_campaign.healPlayer(healAmount);
        appendLog(QString("[调试] 回复 %1 HP，当前 HP: %2")
            .arg(healAmount).arg(m_campaign.playerHp()));
    });

    // --- 按钮5: 添加筹码 ---
    QPushButton* chipsBtn = new QPushButton("添加 500 筹码");
    layout->addWidget(chipsBtn);
    connect(chipsBtn, &QPushButton::clicked, [&]() {
        dlg.accept();
        m_campaign.addChips(500);
        appendLog(QString("[调试] 添加 500 筹码，当前: %1").arg(m_campaign.playerChips()));
    });

    // --- 按钮6: 关闭 ---
    QPushButton* closeBtn = new QPushButton("关闭");
    closeBtn->setStyleSheet("QPushButton { background-color: #5a2a2a; } QPushButton:hover { background-color: #7a3a3a; }");
    layout->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.exec();
}