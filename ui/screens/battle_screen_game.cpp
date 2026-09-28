#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include <QDebug>
#include <map>
#include <QRandomGenerator>
#include "core/variant/variant_registry.h"
#include "ai/engine/ai_levels.h"
#include "core/card/cardtype.h"
#include "ai/players/ai2_liar.h"
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
    m_dealAnimFinishCount++;
    qDebug() << "[onDealAnimationFinished] count=" << m_dealAnimFinishCount
             << "playerAIsFirst=" << m_playerAIsFirst
             << "phase=" << static_cast<int>(m_phase);
    m_difficultyButton->setEnabled(true);
    m_newGameButton->setEnabled(true);
    m_buttonStack->setEnabled(true);
    m_buttonStack->setCurrentIndex(0);

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

    qDebug() << "[startNewGame] before cleanup: A=" << (int)m_playerACardWidgets.size()
             << "B=" << (int)m_playerBCardWidgets.size();

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
        auto children = m_tableCardsWidget->findChildren<QLabel*>();
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

    m_playerA = createPlayer("玩家A");
    m_playerB = createPlayer("电脑");
    m_playerA.isHuman = true;

    if (m_isLevelMode) {
        if (m_currentLevel >= 3) {
            m_levelEngineConfig = buildAIEngineConfig(m_currentLevel);
            m_levelAIEngine = std::make_unique<AIEngine>(m_levelEngineConfig);
            m_levelAIEngine->setOpponentHand(m_playerA.hand);
        }
        m_playerB.aiLevel = AILevel::AI4_Expert;
        m_tracker.reset();
        m_roundCount = 1;

        m_campaign.startLevel(m_currentLevel);

        m_logTextEdit->clear();
        appendLog(QString("========== 关卡模式 - 第 %1 关 ==========").arg(m_currentLevel));
        appendLog(QString("对手: %1").arg(getLevelDisplayName(m_currentLevel)));
        appendLog("请选择先手方...");

        // 关卡模式：不显示先后手选择，随机先手
        if (m_firstChoiceWidget) {
            m_firstChoiceWidget->setVisible(false);
        }
        m_waitingForFirstChoice = false;
        m_pendingPick = false;

        m_playerAIsFirst = (QRandomGenerator::global()->bounded(2) == 0);

        m_pickedCards.clear();

        // 双牌堆系统：玩家游玩牌堆 + 隐藏牌堆（关卡模式专属）
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
        // Boss牌堆（按关卡剔牌，不变）
        {
            auto bossCards = removeCards(createStandardDeck().cards, getLevelRemoveTable(m_currentLevel));
            m_bossDeck.cards = drawRandom(bossCards, 27);
        }

        executeCheats(CheatWhen::BeforeDeal);

        dealCards(m_playerA, m_playerDeck, kMaxHandSize);
        sortHandSmart(m_playerA.hand);
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
    qDebug() << "[updateUI] enter, rebuildHand=" << rebuildHand
             << "hideNewWidgets=" << hideNewWidgets
             << "phase=" << static_cast<int>(m_phase)
             << "playerA.hand=" << (int)m_playerA.hand.size()
             << "playerAWidgets=" << (int)m_playerACardWidgets.size()
             << "playerB.hand=" << (int)m_playerB.hand.size()
             << "playerBWidgets=" << (int)m_playerBCardWidgets.size()
             << "handWidgetW=" << (m_playerAHandWidget ? m_playerAHandWidget->width() : -1)
             << "handWidgetH=" << (m_playerAHandWidget ? m_playerAHandWidget->height() : -1);

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

        for (auto& [seq, cw] : widgetBySeq) {
            cw->setParent(nullptr);
            delete cw;
        }
        for (auto& [key, cw] : widgetByFallback) {
            cw->setParent(nullptr);
            delete cw;
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

bool BattleScreen::checkGameEnd(Player& finisher, Player& opponent)
{
    if (!finisher.hand.empty()) return false;

    // 双牌堆：检查该玩家的牌堆是否为空
    Deck& finisherDeck = (finisher.name == m_playerA.name) ? m_playerDeck : m_bossDeck;
    if (!finisherDeck.cards.empty()) return false;

    endRound(finisher);
    finalSettlement(finisher, opponent, m_tableCards);
    compareAndAnnounce(m_playerA, m_playerB);

    appendLog("========== 游戏结束 ==========");
    appendLog(QString("出完牌者: %1").arg(QString::fromStdString(finisher.name)));
    appendLog(QString("玩家A 总分: %1").arg(m_playerA.totalScore));
    appendLog(QString("电脑 总分: %1").arg(m_playerB.totalScore));

    QString w = (m_playerA.totalScore >= m_playerB.totalScore)
        ? "玩家A" : "电脑";
    appendLog(QString("最终胜者: %1").arg(w));

    showGameOverDialog(QString("%1 出完牌！\n玩家A: %2 分\n电脑: %3 分")
        .arg(QString::fromStdString(finisher.name))
        .arg(m_playerA.totalScore)
        .arg(m_playerB.totalScore));
    return true;
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
    dlg.setWindowTitle("游戏结束");
    dlg.setMinimumSize(420, 360);
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

    auto* title = new QLabel("游戏结束");
    QFont tf = title->font();
    tf.setPointSize(22);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("QLabel { color: #FFD700; }");
    layout->addWidget(title);

    auto* finisher = new QLabel(message.split("\n").first());
    finisher->setAlignment(Qt::AlignCenter);
    finisher->setStyleSheet("QLabel { color: #FFFFFF; font-size: 16px; }");
    layout->addWidget(finisher);

    int scoreA = m_playerA.totalScore;
    int scoreB = m_playerB.totalScore;
    int diff = qAbs(scoreA - scoreB);
    bool aWin = scoreA > scoreB;
    bool tie = scoreA == scoreB;

    auto* labelA = new QLabel(QString("玩家A: %1 分").arg(scoreA));
    auto* labelB = new QLabel(QString("电脑: %1 分").arg(scoreB));
    labelA->setAlignment(Qt::AlignCenter);
    labelB->setAlignment(Qt::AlignCenter);

    if (!tie) {
        QString winStyle = "QLabel { color: #FFD700; font-size: 20px; font-weight: bold; }";
        QString loseStyle = "QLabel { color: #B0BEC5; font-size: 18px; }";
        labelA->setStyleSheet(aWin ? winStyle : loseStyle);
        labelB->setStyleSheet(aWin ? loseStyle : winStyle);
    } else {
        labelA->setStyleSheet("QLabel { color: #FFD700; font-size: 20px; font-weight: bold; }");
        labelB->setStyleSheet("QLabel { color: #FFD700; font-size: 20px; font-weight: bold; }");
    }
    layout->addWidget(labelA);
    layout->addWidget(labelB);

    QString resultText;
    if (tie) resultText = "平局";
    else resultText = QString("胜者: %1（领先 %2 分）")
        .arg(aWin ? "玩家A" : "电脑").arg(diff);
    auto* result = new QLabel(resultText);
    result->setAlignment(Qt::AlignCenter);
    result->setStyleSheet("QLabel { color: #FFF59D; font-size: 16px; font-weight: bold; }");
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

    auto* cardsA = new QLabel(QString("玩家A 分值牌: %1").arg(countScoreCards(m_playerA.collected)));
    auto* cardsB = new QLabel(QString("电脑 分值牌: %1").arg(countScoreCards(m_playerB.collected)));
    cardsA->setAlignment(Qt::AlignCenter);
    cardsB->setAlignment(Qt::AlignCenter);
    cardsA->setStyleSheet("QLabel { color: #B0BEC5; font-size: 14px; }");
    cardsB->setStyleSheet("QLabel { color: #B0BEC5; font-size: 14px; }");
    layout->addWidget(cardsA);
    layout->addWidget(cardsB);

    layout->addStretch();

    auto* btnLayout = new QHBoxLayout;
    auto* againBtn = new QPushButton("再来一局");
    auto* exitBtn = new QPushButton("退出");
    btnLayout->addWidget(againBtn);
    btnLayout->addWidget(exitBtn);
    layout->addLayout(btnLayout);

    connect(againBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(exitBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    int ret = dlg.exec();

    if (ret == QDialog::Accepted) {
        m_phase = GamePhase::DealAnimation;
        onNewGameButtonClicked();
    }
}

void BattleScreen::startNextTurn()
{
    if (m_stressPlaying) {
        m_isStressPlayerATurn = !m_isStressPlayerATurn;
        m_phase = GamePhase::AITurn;
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
        return;
    }
    if (!m_isLevelMode || m_playerAIsFirst) {
        m_phase = GamePhase::PlayerTurn;
        enableActionButtons();
    } else {
        m_phase = GamePhase::AITurn;
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
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