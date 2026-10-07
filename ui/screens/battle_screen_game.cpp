#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/dialogue_overlay.h"
#include "ui/audio/audio_manager.h"
#include "dialogue/dialogue_engine.h"
#include "game/level_boss.h"
#include <QDebug>
#include <map>
#include <QRandomGenerator>
#include "core/variant/variant_registry.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"
#include "game/campaign.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QTimer>
#include <QParallelAnimationGroup>
#include <QGraphicsDropShadowEffect>
#include <random>
#include <algorithm>

void BattleScreen::startGameWithFirst(bool playerAFirst)
{
    if (!m_pendingPick) return;
    m_pendingPick = false;
    m_playerAIsFirst = playerAFirst;

    if (m_firstChoiceWidget) m_firstChoiceWidget->hide();

    // 从双牌堆中移除玩家选的牌
    for (const Card& c : m_pickedCards) {
        auto it = std::find_if(m_playerDeck.cards.begin(), m_playerDeck.cards.end(),
            [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
        if (it != m_playerDeck.cards.end()) m_playerDeck.cards.erase(it);
    }

    shuffleDeck(m_playerDeck);
    shuffleDeck(m_bossDeck);
    emit gameStarted();

    m_playerA.hand = m_pickedCards;
    int needA = kMaxHandSize - (int)m_playerA.hand.size();
    if (needA > 0) dealCards(m_playerA, m_playerDeck, needA);
    sortHandSmart(m_playerA.hand);

    dealCards(m_playerB, m_bossDeck, kMaxHandSize);

    appendLog(QString("玩家A 手牌: %1").arg(cardsToString(m_playerA.hand)));
    appendLog(QString("电脑 手牌: %1").arg(cardsToString(m_playerB.hand)));
    appendLog(QString("玩家牌堆: %1 张 | Boss牌堆: %2 张")
        .arg((int)m_playerDeck.cards.size()).arg((int)m_bossDeck.cards.size()));
    appendLog(QString("先手: %1").arg(playerAFirst ? "玩家A" : "电脑"));

    m_difficultyButton->setEnabled(false);
    m_newGameButton->setEnabled(false);
    m_buttonStack->setEnabled(false);
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);

    playDealAnimation();

    connect(this, &BattleScreen::dealAnimationFinished, this, &BattleScreen::onDealAnimationFinished, Qt::SingleShotConnection);
}

void BattleScreen::onDealAnimationFinished()
{
    m_difficultyButton->setEnabled(true);
    m_newGameButton->setEnabled(true);
    m_buttonStack->setEnabled(true);
    m_buttonStack->setCurrentIndex(0);

    // 播放战斗BGM
    AUDIO_MGR.playBgm(AudioManager::BgmTrack::Battle);

    if (m_playerAIsFirst) {
        m_phase = GamePhase::PlayerTurn;
        enableActionButtons();
    } else {
        m_phase = GamePhase::AITurn;
        disableActionButtons();
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
    }
    updateUI(false);
    layoutHandSlots(true, false);
    layoutHandSlots(false, false);
}

void BattleScreen::startNewGame()
{
    VariantRegistry::clear();
    m_handDirty = true;

    for (auto* g : m_activeRefillGroups) {
        g->stop();
        g->deleteLater();
    }
    m_activeRefillGroups.clear();

    for (CardWidget* cw : m_playerACardWidgets) {
        cw->setParent(nullptr);
        delete cw;
    }
    m_playerACardWidgets.clear();

    for (QWidget* w : m_playerBCardWidgets) {
        w->setParent(nullptr);
        delete w;
    }
    m_playerBCardWidgets.clear();

    for (CardWidget* cw : m_tableCardWidgets) cw->deleteLater();
    m_tableCardWidgets.clear();

    {
        auto children = m_tableCardsWidget->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        for (QLabel* lbl : children) lbl->deleteLater();
    }

    m_pendingPick = true;
    m_lastPlay = CardTypeResult{};
    m_tableCards.clear();
    m_tableBonus = 0;
    m_lastPlayerName.clear();
    m_pickedCards.clear();
    m_playerPlayedCards.clear();
    m_pendingSpecialVictory = false;
    m_playerPlayedSpecial523 = false;
    m_playerWonLastRound = false;
    m_shopPurchasedCards.clear();
    m_shopHandCards.clear();

    if (m_scoreALabel) m_scoreALabel->setText("玩家A: 0 分");
    if (m_scoreBLabel) m_scoreBLabel->setText("电脑: 0 分");
    if (m_tableScoreLabel) m_tableScoreLabel->setText("原始分: 0 分 | 奖励分: 0 分 | 合计: 0 分");
    if (m_handTypeLabel) m_handTypeLabel->setText("上一手牌型: 无");
    if (!m_tableHintLabel.isNull()) m_tableHintLabel->hide();

    m_playerA = createPlayer("玩家A");
    m_playerB = createPlayer("电脑");
    m_playerA.isHuman = true;

    // 关卡模式：每一关开局都通过猜牌决定先手
    // 猜大鬼或小鬼，猜对玩家先手，猜错电脑先手
    if (m_isLevelMode) {
        m_playerB.aiLevel = AILevel::AI1_Rule;
        m_tracker.reset();
        m_roundCount = 1;

        // 重新开始第一关时恢复满血并重置金币
        if (m_currentLevel == 1) {
            m_campaign.startNew();
        }
        m_campaign.startLevel(m_currentLevel);

        // ── 筹码：扣除入场费 + 更新显示 ──
        int entryChips = getLevelEntryChips(m_currentLevel);
        if (m_campaign.spendChips(entryChips)) {
            appendLog(QString("入场筹码: -%1").arg(entryChips));
        }
        if (m_chipsLabel) {
            m_chipsLabel->setText(QString("🎡 筹码: %1").arg(m_campaign.playerChips()));
            m_chipsLabel->show();
        }
        if (m_entryChipsLabel) {
            m_entryChipsLabel->setText(QString("本局入场: %1 筹码  ·  胜利奖励: %2 筹码")
                .arg(entryChips).arg(getLevelRewardChips(m_currentLevel)));
            m_entryChipsLabel->show();
        }
        if (m_hpLabel) {
            // HP在关卡模式中显示（updateLabels 内含 HP 文本刷新）
            updateLabels();
        }
        // ── 筹码 END ──

        m_logTextEdit->clear();
        appendLog(QString("========== 关卡模式 - 第 %1 关 ==========").arg(m_currentLevel));
        appendLog(QString("对手: %1").arg(getLevelDisplayName(m_currentLevel)));
        appendLog("请选择先手方...");

        // 隐藏练习模式的选先手控件
        if (m_firstChoiceWidget) {
            m_firstChoiceWidget->setVisible(false);
        }
        m_waitingForFirstChoice = false;
        m_pendingPick = false;

        // 重置对话标记
        resetDialogueFlags();

        // 播放战前剧情对话 → 结束后进入猜牌环节
        auto preLines = DialogueEngine::instance().getDialogue(m_currentLevel, DialogueTrigger::PreBattle);
        if (!preLines.isEmpty() && m_dialogueOverlay) {
            m_dialogueCtx = DialogueCtx::PreBattle;
            m_dialogueOverlay->playDialogue(preLines, m_currentLevel);
            // 注意：对话结束后 onDialogueFinished() 会调用 showGuessWidget()
        } else {
            showGuessWidget();
        }
        return;
    }

    m_playerB.aiLevel = m_aiLevel;
    m_tracker.reset();
    m_roundCount = 1;

    // 练习模式也使用双牌堆（各自独立54张标准牌，各洗各的，可能持有相同牌）
    {
        auto playerCards = createStandardDeck().cards;
        m_playerDeck.cards = drawRandom(playerCards, 27);
        auto bossCards = createStandardDeck().cards;
        m_bossDeck.cards = drawRandom(bossCards, 27);
    }

    m_difficultyButton->setVisible(true);
    m_difficultyButton->setEnabled(true);

    m_logTextEdit->clear();
    appendLog("========== 新游戏开始 ==========");
    appendLog("请选择先手方（可先选卡）");

    if (m_firstChoiceWidget) {
        m_firstChoiceWidget->setGeometry(m_tableCardsWidget->rect());
        m_firstChoiceWidget->setVisible(true);
        m_firstChoiceWidget->raise();
        m_waitingForFirstChoice = true;
    }

    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);

    updateUI();
}

void BattleScreen::updateUI(bool rebuildHand, bool hideNewWidgets)
{
    updateLabels();

    updateTableHint();

    if (m_phase == GamePhase::DealAnimation || m_phase == GamePhase::RefillAnimation) return;

    if (rebuildHand) {
        rebuildPlayerHands(hideNewWidgets);
    }

    updateButtonStates();
}

void BattleScreen::updateLabels()
{
    int playerDeckCount = static_cast<int>(m_playerDeck.cards.size());
    int bossDeckCount = static_cast<int>(m_bossDeck.cards.size());

    if (m_isLevelMode) {
        m_deckCountLabel->setText(
            QString("关卡: %1/9").arg(m_currentLevel));
        m_deckCountLabel->setVisible(true);
    } else {
        m_deckCountLabel->setVisible(false);
    }

    if (m_playerDeckCountLabel) {
        m_playerDeckCountLabel->setText(QString("%1 张").arg(playerDeckCount));
    }
    if (m_bossDeckCountLabel) {
        m_bossDeckCountLabel->setText(QString("%1 张").arg(bossDeckCount));
    }
    m_roundLabel->setText(
        QString("回合: %1").arg(m_roundCount));

    if (m_titleLabel) {
        if (m_isLevelMode) {
            m_titleLabel->setText(
                QString("关卡模式 · 第 %1/9 关").arg(m_currentLevel));
        } else {
            m_titleLabel->setText("练习模式");
        }
    }

    m_scoreALabel->setText(
        QString("玩家A: %1 分").arg(m_playerA.totalScore));
    m_scoreBLabel->setText(
        QString("电脑: %1 分").arg(m_playerB.totalScore));

    // 关卡模式隐藏分数，只显示血量
    if (m_scoreALabel)   m_scoreALabel->setVisible(!m_isLevelMode);
    if (m_scoreBLabel)   m_scoreBLabel->setVisible(!m_isLevelMode);
    if (m_tableScoreLabel) m_tableScoreLabel->setVisible(!m_isLevelMode);
    if (m_handTypeLabel) m_handTypeLabel->setVisible(!m_isLevelMode);

    if (m_hpLabel && m_isLevelMode) {
        m_hpLabel->setText(QString("血量: 玩家 %1/%2  |  Boss %3/%4")
            .arg(m_campaign.playerHp())
            .arg(m_campaign.config().playerMaxHp)
            .arg(m_campaign.bossHp())
            .arg(m_campaign.config().bossMaxHp));
        qDebug() << "[updateLabels] hp text=" << m_hpLabel->text();
        m_hpLabel->setVisible(true);
    } else if (m_hpLabel) {
        m_hpLabel->setVisible(false);
    }

    if (m_lastPlay.type != CardType::Invalid) {
        QString typeStr = cardTypeToQString(m_lastPlay.type);
        if (!m_lastPlay.keyPoint.empty())
            typeStr += QString(" [%1]").arg(QString::fromStdString(m_lastPlay.keyPoint));
        m_handTypeLabel->setText(
            QString("上一手牌型: %1（%2）").arg(typeStr)
                .arg(m_lastPlayerName));
    } else {
        m_handTypeLabel->setText("上一手牌型: 无");
    }

    int originalScore = calculateScore(m_tableCards);
    int tableScore = calculateTableScore(m_tableCards, m_tableBonus);
    m_tableScoreLabel->setText(QString("原始分: %1 分 | 奖励分: %2 分 | 合计: %3 分")
        .arg(originalScore)
        .arg(m_tableBonus)
        .arg(tableScore));
}

void BattleScreen::updateTableHint()
{
    bool needHint = (!m_waitingForFirstChoice && !m_pendingPick
                      && m_tableCardWidgets.empty()
                      && (m_lastPlay.type == CardType::Invalid
                          || m_lastPlay.cards.empty()));
    if (needHint) {
        if (m_tableHintLabel.isNull()) {
            m_tableHintLabel = new QLabel("等待出牌", m_tableCardsWidget);
            m_tableHintLabel->setAlignment(Qt::AlignCenter);
            m_tableHintLabel->setStyleSheet("QLabel { color: #F5A623; }");
            QFont hintFont = m_tableHintLabel->font();
            hintFont.setPointSize(16);
            m_tableHintLabel->setFont(hintFont);
        }
        m_tableHintLabel->setGeometry(0, 0,
            m_tableCardsWidget->width(), m_tableCardsWidget->height());
        m_tableHintLabel->show();
        m_tableHintLabel->raise();
    } else if (!m_tableHintLabel.isNull()) {
        m_tableHintLabel->hide();
    }
}

void BattleScreen::rebuildPlayerHands(bool hideNewWidgets)
{
    if (!m_handDirty && m_playerACardWidgets.size() == m_playerA.hand.size()) {
        qDebug() << "[rebuildPlayerHands] skip (not dirty, sizes match)";
        return;
    }
    qDebug() << "[rebuildPlayerHands] enter, hideNewWidgets=" << hideNewWidgets
             << "phase=" << static_cast<int>(m_phase)
             << "m_playerACardWidgets.size=" << (int)m_playerACardWidgets.size();
    for (size_t i = 0; i < m_playerACardWidgets.size(); ++i) {
        CardWidget* cw = m_playerACardWidgets[i];
        qDebug() << "  [" << i << "] cw=" << static_cast<void*>(cw)
                 << "parent=" << static_cast<void*>(cw ? cw->parentWidget() : nullptr)
                 << "isHandWidget=" << (cw && cw->parentWidget() == m_playerAHandWidget)
                 << "isThis=" << (cw && cw->parentWidget() == this);
    }
    {
        std::map<int, CardWidget*> widgetBySeq;
        std::map<std::pair<std::string, std::string>, CardWidget*> widgetByFallback;
        for (CardWidget* cw : m_playerACardWidgets) {
            if (!cw) continue;
            if (cw->parentWidget() != m_playerAHandWidget) {
                qDebug() << "[rebuildPlayerHands] cw=" << static_cast<void*>(cw)
                         << "parent NOT handWidget! parent="
                         << static_cast<void*>(cw->parentWidget())
                         << "isThis=" << (cw->parentWidget() == this);
            }
            const Card& c = cw->getCard();
            if (c.seq > 0) {
                widgetBySeq[c.seq] = cw;
            } else {
                widgetByFallback[{c.point, c.suit}] = cw;
            }
        }

        qDebug() << "[rebuildPlayerHands] mapSize=" << (int)widgetBySeq.size()
                 << "handSize=" << (int)m_playerA.hand.size();

        std::vector<CardWidget*> newOrder;
        for (const Card& hc : m_playerA.hand) {
            CardWidget* reuse = nullptr;
            if (hc.seq > 0) {
                auto it = widgetBySeq.find(hc.seq);
                if (it != widgetBySeq.end()) {
                    reuse = it->second;
                    widgetBySeq.erase(it);
                }
            }
            if (!reuse) {
                auto it = widgetByFallback.find({hc.point, hc.suit});
                if (it != widgetByFallback.end()) {
                    reuse = it->second;
                    widgetByFallback.erase(it);
                }
            }
            if (reuse) {
                reuse->setCard(hc);
                if (reuse->parentWidget() != m_playerAHandWidget) {
                    reuse->setParent(m_playerAHandWidget);
                    reuse->show();
                }
                newOrder.push_back(reuse);
            } else {
                CardWidget* nw = new CardWidget(hc, m_playerAHandWidget);
                connect(nw, &CardWidget::clicked, this, [this]() { update(); });
                if (!hideNewWidgets) {
                    nw->show();
                } else {
                    nw->setVisible(false);
                }
                newOrder.push_back(nw);
            }
        }

        for (auto& pair : widgetBySeq) {
            pair.second->setParent(nullptr);
            delete pair.second;
        }
        for (auto& pair : widgetByFallback) {
            pair.second->setParent(nullptr);
            delete pair.second;
        }

        m_playerACardWidgets = std::move(newOrder);
        layoutHandSlots(true, false);
    }

    {
        while (m_playerBCardWidgets.size() > m_playerB.hand.size()) {
            QWidget* w = m_playerBCardWidgets.back();
            w->setParent(nullptr);
            delete w;
            m_playerBCardWidgets.pop_back();
        }
        while (m_playerBCardWidgets.size() < m_playerB.hand.size()) {
            QWidget* w = createCardBack();
            w->setParent(m_playerBHandWidget);
            if (!hideNewWidgets) {
                w->show();
            } else {
                w->setVisible(false);
            }
            m_playerBCardWidgets.push_back(w);
        }
        layoutHandSlots(false, false);
    }

    m_handDirty = false;

    if (m_playerAHandWidget) m_playerAHandWidget->update();
    if (m_playerBHandWidget) m_playerBHandWidget->update();
}

void BattleScreen::updateButtonStates()
{
    if (m_phase != GamePhase::GameOver) {
        if (m_pendingPick) {
            m_buttonStack->setCurrentIndex(1);
            m_playButton->setEnabled(false);
        } else {
            m_buttonStack->setCurrentIndex(0);
            m_playButton->setEnabled(m_phase == GamePhase::PlayerTurn && !m_playerA.hand.empty());
        }
        m_passButton->setEnabled(m_lastPlay.type != CardType::Invalid);
    }
}

void BattleScreen::showGameOverDialog(const QString& message)
{
    qDebug() << "[showGameOverDialog] phase=" << static_cast<int>(m_phase);
    m_phase = GamePhase::GameOver;
    disableActionButtons();

    if (m_stressPlaying) {
        qDebug() << "[STRESS] game finished, success";
        m_stressSuccessGames++;
        QTimer::singleShot(500, this, [this]() {
            autoPlayOneGame();
        });
        return;
    }

    // 停止战斗BGM（胜负BGM在比分计算后再决定）
    AUDIO_MGR.stopBgm(300);

    if (m_isLevelMode) {
        RoundResult rr;
        rr.specialVictory = m_pendingSpecialVictory;
        LevelResult lr = m_campaign.finishLevel(
            rr, m_playerA, m_playerB,
            static_cast<int>(m_playerDeck.cards.size()),
            static_cast<int>(m_bossDeck.cards.size()));
        handleLevelModeEnd(lr);
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("牌局结算");
    dlg.setMinimumSize(460, 420);
    dlg.setStyleSheet(R"(
        QDialog {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #2a151a, stop:0.5 #1a0d10, stop:1 #0f0808);
            border: 2px solid;
            border-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #8b6508, stop:0.5 #ffd700, stop:1 #8b6508);
            border-radius: 12px;
        }
        QLabel { color: #fff8e7; font-size: 14px; }
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #c41e3a, stop:1 #8b0000);
            color: #ffd700;
            border: 2px solid #ffd700;
            border-radius: 8px;
            padding: 10px 28px;
            font-size: 15px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #dc3545, stop:1 #a01520);
        }
        QPushButton:pressed {
            background-color: #8b0000;
        }
    )");

    QGraphicsDropShadowEffect* dlgShadow = new QGraphicsDropShadowEffect(&dlg);
    dlgShadow->setBlurRadius(30);
    dlgShadow->setOffset(0, 12);
    dlgShadow->setColor(QColor(0, 0, 0, 200));
    dlg.setGraphicsEffect(dlgShadow);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(10);

    // 顶部装饰：祥云纹标题
    auto* title = new QLabel("🏆  牌局结算  🏆");
    QFont tf = title->font();
    tf.setPointSize(24);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(R"(
        QLabel {
            color: #ffd700;
            font-size: 24px;
            font-weight: bold;
            letter-spacing: 4px;
        }
    )");
    layout->addWidget(title);

    // 分割线
    auto* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("QFrame { color: rgba(255,215,0,0.4); }");
    layout->addWidget(sep1);

    auto* finisher = new QLabel(message.split("\n").first());
    finisher->setAlignment(Qt::AlignCenter);
    finisher->setStyleSheet(R"(
        QLabel {
            color: #ffe66d;
            font-size: 15px;
            padding: 4px 0;
        }
    )");
    layout->addWidget(finisher);

    int scoreA = m_playerA.totalScore;
    int scoreB = m_playerB.totalScore;
    int diff = qAbs(scoreA - scoreB);
    bool aWin = scoreA > scoreB;
    bool tie = scoreA == scoreB;

    // 播放胜负BGM（延迟到淡出结束后）
    if (!tie) {
        auto track = aWin ? AudioManager::BgmTrack::Victory : AudioManager::BgmTrack::Defeat;
        QTimer::singleShot(350, this, [track]() {
            AUDIO_MGR.playBgm(track);
        });
    }

    // 比分面板（国潮风卡片）
    auto* scorePanel = new QWidget;
    scorePanel->setStyleSheet(R"(
        QWidget {
            background-color: rgba(255, 215, 0, 0.06);
            border: 1px solid rgba(255, 215, 0, 0.25);
            border-radius: 10px;
        }
    )");
    auto* scoreRow = new QHBoxLayout(scorePanel);
    scoreRow->setContentsMargins(16, 12, 16, 12);
    scoreRow->setSpacing(8);

    auto* labelA = new QLabel(QString("玩家A\n%1 分").arg(scoreA));
    auto* labelB = new QLabel(QString("电脑\n%1 分").arg(scoreB));
    labelA->setAlignment(Qt::AlignCenter);
    labelB->setAlignment(Qt::AlignCenter);
    labelA->setWordWrap(true);
    labelB->setWordWrap(true);

    if (!tie) {
        QString winStyle = R"(
            QLabel {
                color: #ffd700;
                font-size: 22px;
                font-weight: bold;
                background: transparent;
            }
        )";
        QString loseStyle = R"(
            QLabel {
                color: #9e9e9e;
                font-size: 18px;
                background: transparent;
            }
        )";
        labelA->setStyleSheet(aWin ? winStyle : loseStyle);
        labelB->setStyleSheet(aWin ? loseStyle : winStyle);
    } else {
        QString tieStyle = R"(
            QLabel {
                color: #ffd700;
                font-size: 22px;
                font-weight: bold;
                background: transparent;
            }
        )";
        labelA->setStyleSheet(tieStyle);
        labelB->setStyleSheet(tieStyle);
    }

    scoreRow->addWidget(labelA, 1);

    auto* vsLabel = new QLabel("VS");
    vsLabel->setAlignment(Qt::AlignCenter);
    vsLabel->setStyleSheet(R"(
        QLabel {
            color: #c9b896;
            font-size: 14px;
            font-weight: bold;
            background: transparent;
        }
    )");
    scoreRow->addWidget(vsLabel);

    scoreRow->addWidget(labelB, 1);
    layout->addWidget(scorePanel);

    // 结果文本
    QString resultText;
    if (tie) resultText = "🤝 平  局";
    else resultText = QString("%1  胜  （领先 %2 分）")
        .arg(aWin ? "🎉 玩家A" : "💻 电脑").arg(diff);
    auto* result = new QLabel(resultText);
    result->setAlignment(Qt::AlignCenter);
    result->setStyleSheet(R"(
        QLabel {
            color: #ffe66d;
            font-size: 17px;
            font-weight: bold;
            padding: 4px 0;
        }
    )");
    layout->addWidget(result);

    auto countScoreCards = [](const std::vector<Card>& collected) -> QString {
        int fives = 0, tens = 0, kings = 0;
        for (const auto& c : collected) {
            if (c.point == "5") fives++;
            else if (c.point == "10") tens++;
            else if (c.point == "K") kings++;
        }
        return QString("5×%1  10×%2  K×%3").arg(fives).arg(tens).arg(kings);
    };

    // 分值牌明细
    auto* cardsInfo = new QWidget;
    cardsInfo->setStyleSheet("QWidget { background: transparent; }");
    auto* cardsLay = new QVBoxLayout(cardsInfo);
    cardsLay->setContentsMargins(0, 0, 0, 0);
    cardsLay->setSpacing(4);

    auto* cardsA = new QLabel(QString("🎴 玩家A 分值牌: %1").arg(countScoreCards(m_playerA.collected)));
    auto* cardsB = new QLabel(QString("🎴 电脑 分值牌: %1").arg(countScoreCards(m_playerB.collected)));
    cardsA->setAlignment(Qt::AlignCenter);
    cardsB->setAlignment(Qt::AlignCenter);
    cardsA->setStyleSheet(R"(
        QLabel {
            color: #c9b896;
            font-size: 13px;
            background: transparent;
        }
    )");
    cardsB->setStyleSheet(R"(
        QLabel {
            color: #c9b896;
            font-size: 13px;
            background: transparent;
        }
    )");
    cardsLay->addWidget(cardsA);
    cardsLay->addWidget(cardsB);
    layout->addWidget(cardsInfo);

    layout->addStretch();

    // 按钮区
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(12);
    auto* againBtn = new QPushButton("再来一局");
    auto* exitBtn = new QPushButton("返回大厅");
    btnLayout->addStretch();
    btnLayout->addWidget(againBtn);
    btnLayout->addWidget(exitBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(againBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(exitBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    int ret = dlg.exec();

    if (ret == QDialog::Accepted) {
        m_phase = GamePhase::DealAnimation;
        onNewGameButtonClicked();
    }
}

// 回合轮转：根据上一回合赢家决定本轮先手
// 第一回合根据猜牌结果，后续回合由上一轮赢家先出
void BattleScreen::startNextTurn()
{
    if (m_stressPlaying) {
        m_isStressPlayerATurn = !m_isStressPlayerATurn;
        m_phase = GamePhase::AITurn;
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
        return;
    }

    bool playerGoesFirst = false;

    // 第一回合：按猜牌结果决定先手；后续回合：由上一轮赢家先出
    if (m_roundCount == 1) {
        playerGoesFirst = m_playerAIsFirst;
    } else {
        playerGoesFirst = m_playerWonLastRound;
    }

    int delayMs = 300;
    if (!m_activeRefillGroups.empty()) {
        delayMs = 550;
    }

    if (playerGoesFirst) {
        m_phase = GamePhase::PlayerTurn;
        appendLog(">>> 玩家A 先出牌");
        QTimer::singleShot(delayMs, this, [this]() {
            enableActionButtons();
        });
    } else {
        m_phase = GamePhase::AITurn;
        disableActionButtons();
        QTimer::singleShot(std::max(delayMs, m_aiTurnDelayMs), this, &BattleScreen::doAITurn);
        appendLog(">>> 电脑 先出牌");
    }
}

void BattleScreen::cheat_loadVariantDeck()
{
    ai2_variant_deck(m_bossDeck.cards);
}

void BattleScreen::cheat_forceFirstHand(bool playerAIsFirst)
{
    m_playerAIsFirst = playerAIsFirst;
}

void BattleScreen::cheat_setHandLimit(int /*limit*/)
{
}

void BattleScreen::cheat_banCard(const std::string& /*point*/)
{
}

void BattleScreen::executeCheats(CheatWhen when)
{
    const auto& table = getCheatTable();
    auto it = table.find(m_currentLevel);
    if (it == table.end()) return;
    for (const auto& entry : it->second) {
        if (entry.when == when) entry.action(this);
    }
}

// 猜牌结束后继续关卡游戏流程
// 初始化双方牌堆、发牌、播放发牌动画
void BattleScreen::continueLevelGameAfterGuess()
{
    m_pickedCards.clear();

    // 首次关卡：创建全新双牌堆（各27张）
    // 后续关卡：从隐藏牌堆补充到27张（剩余牌继承）
    if (m_playerHiddenDeck.cards.empty()) {
        auto playerFull = createStandardDeck().cards;
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(playerFull.begin(), playerFull.end(), g);
        m_playerDeck.cards.clear();
        m_playerHiddenDeck.cards.clear();
        for (int i = 0; i < 54; ++i) {
            playerFull[i].seq = i;
            if (i < 27) m_playerDeck.cards.push_back(playerFull[i]);
            else m_playerHiddenDeck.cards.push_back(playerFull[i]);
        }
        m_playerNextSeq = 54;
    } else {
        std::sort(m_playerHiddenDeck.cards.begin(), m_playerHiddenDeck.cards.end(),
            [](const Card& a, const Card& b) { return a.seq < b.seq; });
        int need = 27 - static_cast<int>(m_playerDeck.cards.size());
        if (need > 0) {
            int take = std::min(need, static_cast<int>(m_playerHiddenDeck.cards.size()));
            m_playerDeck.cards.insert(m_playerDeck.cards.end(),
                m_playerHiddenDeck.cards.begin(),
                m_playerHiddenDeck.cards.begin() + take);
            m_playerHiddenDeck.cards.erase(
                m_playerHiddenDeck.cards.begin(),
                m_playerHiddenDeck.cards.begin() + take);
        }
        std::sort(m_playerDeck.cards.begin(), m_playerDeck.cards.end(),
            [](const Card& a, const Card& b) { return a.seq < b.seq; });
    }

    {
        auto bossCards = removeCards(createStandardDeck().cards, getLevelRemoveTable(m_currentLevel));
        m_bossDeck.cards = drawRandom(bossCards, 27);
    }

    executeCheats(CheatWhen::BeforeDeal);

    // 将从商店购买的卡牌加入玩家牌堆
    if (!m_shopPurchasedCards.empty()) {
        for (auto& c : m_shopPurchasedCards) {
            c.seq = m_playerNextSeq++;
            m_playerDeck.cards.push_back(c);
        }
        appendLog(QString("商店卡牌已加入牌堆: %1 张").arg((int)m_shopPurchasedCards.size()));
        m_shopPurchasedCards.clear();
    }

    dealCards(m_playerA, m_playerDeck, kMaxHandSize);
    sortHandSmart(m_playerA.hand);

    // 商店购买的"进入手牌"卡牌直接加入手牌
    if (!m_shopHandCards.empty()) {
        for (auto& c : m_shopHandCards) {
            c.seq = m_playerNextSeq++;
            m_playerA.hand.push_back(c);
        }
        appendLog(QString("商店手牌已加入: %1 张").arg((int)m_shopHandCards.size()));
        m_shopHandCards.clear();
    }

    dealCards(m_playerB, m_bossDeck, kMaxHandSize);

    executeCheats(CheatWhen::AfterDeal);

    appendLog(QString("玩家A 手牌: %1").arg(cardsToString(m_playerA.hand)));
    appendLog(QString("电脑 手牌: %1").arg(cardsToString(m_playerB.hand)));
    appendLog(QString("玩家牌堆: %1 张 | Boss牌堆: %2 张")
        .arg((int)m_playerDeck.cards.size()).arg((int)m_bossDeck.cards.size()));
    appendLog(QString("先手: %1").arg(m_playerAIsFirst ? "玩家A" : "电脑"));

    if (m_deckCountLabel) {
        m_deckCountLabel->setText(QString("关卡: %1/9").arg(m_currentLevel));
    }
    if (m_roundLabel) {
        m_roundLabel->setText(QString("对手: %1").arg(getLevelDisplayName(m_currentLevel)));
    }

    m_difficultyButton->setEnabled(false);
    m_difficultyButton->setVisible(false);
    m_newGameButton->setEnabled(true);
    m_returnMenuButton->setVisible(true);
    m_buttonStack->setEnabled(false);
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);

    playDealAnimation();

    connect(this, &BattleScreen::dealAnimationFinished, this, &BattleScreen::onDealAnimationFinished, Qt::SingleShotConnection);
}