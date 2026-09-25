#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/dialogs/card_picker.h"
#include "ai/ai.h"
#include "ai/players/ai1_idiot.h"
#include <QDebug>
#include <map>
#include <QRandomGenerator>
#include "ai/players/ai2_liar.h"
#include "core/variant/variant_registry.h"
#include "ai/engine/ai_levels.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"
#include <QParallelAnimationGroup>
#include "game/campaign.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDialog>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <random>
#include <algorithm>

void BattleScreen::onPlayButtonClicked()
{
    if (m_phase != GamePhase::PlayerTurn) return;

    std::vector<Card> selected;
    for (CardWidget* cw : m_playerACardWidgets) {
        if (cw->isSelected()) {
            selected.push_back(cw->getCard());
        }
    }

    if (selected.empty()) {
        QMessageBox::warning(this, "提示", "请先选择要出的牌");
        return;
    }

    CardTypeResult result = parseCardType(selected);
    if (result.type == CardType::Invalid) {
        QMessageBox::warning(this, "非法牌型", "你选的牌不构成合法牌型");
        playSound(m_soundWrong);
        return;
    }

    if (m_lastPlay.type != CardType::Invalid && !canBeat(result, m_lastPlay)) {
        QMessageBox::warning(this, "无法压过", "你选的牌无法压过上一手");
        playSound(m_soundWrong);
        return;
    }

    std::vector<CardWidget*> selectedWidgets;
    for (CardWidget* cw : m_playerACardWidgets) {
        if (cw->isSelected()) {
            selectedWidgets.push_back(cw);
        }
    }

    for (CardWidget* cw : selectedWidgets) {
        auto it = std::find(m_playerACardWidgets.begin(),
                            m_playerACardWidgets.end(), cw);
        if (it != m_playerACardWidgets.end()) {
            m_playerACardWidgets.erase(it);
        }
    }

    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();

    flyCardsToTable(selectedWidgets);

    for (const Card& c : selected) {
        auto it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
            [&](const Card& h) {
                return h.point == c.point && h.suit == c.suit;
            });
        if (it != m_playerA.hand.end()) m_playerA.hand.erase(it);
    }
    m_handDirty = true;

    if (result.type == CardType::Bomb || result.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
    }

    for (const Card& c : selected) {
        m_tableCards.push_back(c);
    }
    if (m_isLevelMode) {
        m_playerPlayedCards.insert(m_playerPlayedCards.end(), selected.begin(), selected.end());
    }
    CardTypeResult oldLastPlay = m_lastPlay;
    m_lastPlay = result;
    m_lastPlayerName = "玩家A";

    m_tracker.recordPlayed(selected, DeckSide::PlayerA);
    if (m_levelAIEngine) {
        m_levelAIEngine->recordPlayed(selected, DeckSide::PlayerA);
    }

    if (result.type == CardType::Special523) {
        appendLog("玩家A 达成七鬼523，直接获胜！");
        playSound(m_soundSuccess);
        m_pendingSpecialVictory = true;
        showSpecialVictoryEffect("玩家A", "玩家A 达成七鬼523，直接获胜！");
        return;
    }

    int bonus = calculatePressureBonus(result, oldLastPlay);
    if (bonus > 0) {
        m_lastPlay.bonusScore = bonus;
        m_tableBonus += bonus;
    }
    if (bonus > 0) showBonusFloat(bonus);

    appendLog(QString("玩家A 出牌: %1 (%2%3)")
        .arg(cardsToString(selected))
        .arg(cardTypeToQString(result.type))
        .arg(m_lastPlay.bonusScore > 0 ? QString(" 压分+%1").arg(m_lastPlay.bonusScore) : ""));

    if (m_lastPlay.bonusScore > 0 || result.type == CardType::Bomb || result.type == CardType::Rocket) {
        playSound(m_soundCasino);
    } else {
        playSound(m_soundCorrect);
    }

    updateUI();

    bool playerAFinished = m_playerA.hand.empty();

    if (playerAFinished && m_playerDeck.cards.empty()) {
        finishGame(m_playerA, "玩家A", m_soundSuccess);
        return;
    }

    if (m_playerB.hand.empty() && !m_bossDeck.cards.empty()) {
        appendLog("玩家A 出牌回应，玩家A 赢得本回合");
        endRound(m_playerA);
        refillBoth(m_playerA, m_playerB);
        clearLastPlay();
        startNextTurn();
        return;
    }

    m_phase = GamePhase::AITurn;
    QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
    if (playerAFinished) {
        m_playButton->setEnabled(false);
    }
}

void BattleScreen::onPassButtonClicked()
{
    playSound(m_soundClick);
    if (m_phase != GamePhase::PlayerTurn) return;

    if (m_lastPlay.type == CardType::Invalid) {
        QMessageBox::warning(this, "提示", "首出不能不要");
        playSound(m_soundWrong);
        return;
    }

    appendLog("玩家A 不要");

    if (m_playerB.hand.empty() && !m_bossDeck.cards.empty()) {
        appendLog("电脑 赢得本回合");
        endRound(m_playerB);
        refillBoth(m_playerB, m_playerA);
        clearLastPlay();
        if (handleSpecialVictoryCheck()) return;
        m_phase = GamePhase::AITurn;
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
        return;
    }

    endRound(m_playerB);

    refillBoth(m_playerB, m_playerA);

    if (handleSpecialVictoryCheck()) return;

    clearLastPlay();
    m_phase = GamePhase::AITurn;
    QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
}

void BattleScreen::onNewGameButtonClicked()
{
    playSound(m_soundClick);
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    m_phase = GamePhase::DealAnimation;
    startNewGame();
}

void BattleScreen::onDifficultyButtonClicked()
{
    playSound(m_soundClick);
    switch (m_aiLevel) {
        case AILevel::AI1_Simple:
            m_aiLevel = AILevel::AI2_Rule;
            m_difficultyButton->setText("难度: 电脑2 骗子");
            break;
        case AILevel::AI2_Rule:
            m_aiLevel = AILevel::AI3_Tracker;
            m_difficultyButton->setText("难度: 电脑3 急性子");
            break;
        case AILevel::AI3_Tracker:
            m_aiLevel = AILevel::AI1_Simple;
            m_difficultyButton->setText("难度: 电脑1 傻子");
            break;
    }
    appendLog(QString("电脑难度切换为: %1").arg(m_difficultyButton->text()));
}

void BattleScreen::onFirstBtnClicked()  { playSound(m_soundClick); startGameWithFirst(true); }
void BattleScreen::onSecondBtnClicked() { playSound(m_soundClick); startGameWithFirst(false); }
void BattleScreen::onRandomBtnClicked() {
    playSound(m_soundClick);
    std::random_device rd; std::mt19937 g(rd());
    std::uniform_int_distribution<> d(0, 1);
    startGameWithFirst(d(g) == 0);
}

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

void BattleScreen::onPickButtonClicked()
{
    playSound(m_soundClick);
    if (!m_pendingPick) {
        QMessageBox::information(this, "提示", "已发牌，不能选卡");
        return;
    }

    CardPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_pickedCards = dlg.selectedCards();
        appendLog(QString("已选 %1 张起始手牌").arg(m_pickedCards.size()));
    }
}

void BattleScreen::doAITurn()
{
    if (m_phase == GamePhase::GameOver) return;

    if (m_stressPlaying) {
        auto sig = std::make_tuple(
            (int)m_playerA.hand.size(),
            (int)m_playerB.hand.size(),
            (int)m_playerDeck.cards.size(),
            (int)m_bossDeck.cards.size()
        );

        if (sig == m_lastTurnSignature) {
            m_stressNoProgressCount++;
            if (m_stressNoProgressCount >= m_stressNoProgressLimit) {
                qWarning() << "[STRESS] GAME FAILED: no progress for"
                           << m_stressNoProgressCount << "turns"
                           << "A.hand=" << m_playerA.hand.size()
                           << "B.hand=" << m_playerB.hand.size()
                           << "A.deck=" << m_playerDeck.cards.size()
                           << "B.deck=" << m_bossDeck.cards.size();
                m_stressFailedGames++;
                m_stressDoAICallCount = 0;
                m_stressNoProgressCount = 0;
                m_lastTurnSignature = {-1,-1,-1,-1};
                m_phase = GamePhase::GameOver;
                QTimer::singleShot(100, this, [this]() {
                    autoPlayOneGame();
                });
                return;
            }
        } else {
            m_stressNoProgressCount = 0;
            m_lastTurnSignature = sig;
        }

        // 兜底：总调用硬上限
        if (++m_stressDoAICallCount > 2000) {
            qWarning() << "[STRESS] GAME FAILED: doAITurn called"
                       << m_stressDoAICallCount << "times, hard limit";
            m_stressFailedGames++;
            m_stressDoAICallCount = 0;
            m_stressNoProgressCount = 0;
            m_lastTurnSignature = {-1,-1,-1,-1};
            m_phase = GamePhase::GameOver;
            QTimer::singleShot(100, this, [this]() {
                autoPlayOneGame();
            });
            return;
        }
    }

    // Determine whose turn it is
    Player* self;
    Player* opp;
    Deck* selfDeck;
    Deck* oppDeck;
    QString selfName;
    QString oppName;
    DeckSide selfSide;
    QMediaPlayer* selfFinishSound;

    if (m_stressPlaying && m_isStressPlayerATurn) {
        self = &m_playerA;  opp = &m_playerB;
        selfDeck = &m_playerDeck;  oppDeck = &m_bossDeck;
        selfName = "玩家A(AI)";  oppName = "电脑";
        selfSide = DeckSide::PlayerA;
        selfFinishSound = m_soundSuccess;
    } else {
        self = &m_playerB;  opp = &m_playerA;
        selfDeck = &m_bossDeck;  oppDeck = &m_playerDeck;
        selfName = "电脑";  oppName = "玩家A";
        selfSide = DeckSide::Boss;
        selfFinishSound = m_soundFailure;
    }

    std::vector<Card> chosen = dispatchAI(*self, *opp, *selfDeck);

    if (chosen.empty()) {
        appendLog(selfName + " 不要");

        if (opp->hand.empty() && !oppDeck->cards.empty()) {
            appendLog(oppName + " 赢得本回合");
            endRound(*opp);
            refillBoth(*opp, *self);
            clearLastPlay();
            startNextTurn();
            return;
        }

        endRound(*opp);

        if (!oppDeck->cards.empty()) {
            refillBoth(*opp, *self);
        } else {
            updateUI();
        }

        if (handleSpecialVictoryCheck()) return;

        clearLastPlay();
        startNextTurn();
        updateUI(false);
        return;
    }

    for (const Card& c : chosen) {
        auto it = std::find_if(self->hand.begin(), self->hand.end(),
            [&](const Card& h) {
                return h.point == c.point && h.suit == c.suit;
            });
        if (it != self->hand.end()) self->hand.erase(it);
    }
    m_handDirty = true;
    for (const Card& c : chosen) {
        m_tableCards.push_back(c);
    }
    CardTypeResult oldLastPlay = m_lastPlay;
    m_lastPlay = parseCardType(chosen);
    m_lastPlayerName = selfName;

    if (m_lastPlay.type == CardType::Bomb || m_lastPlay.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
    }

    if (m_levelAIEngine) m_levelAIEngine->recordPlayed(chosen, selfSide);
    m_tracker.recordPlayed(chosen, selfSide);

    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    flyAICardsToTable(chosen);

    if (m_lastPlay.type == CardType::Special523) {
        appendLog(selfName + " 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showSpecialVictoryEffect(selfName, selfName + " 达成七鬼523，直接获胜！");
        return;
    }

    int bonus = calculatePressureBonus(m_lastPlay, oldLastPlay);
    if (bonus > 0) {
        m_lastPlay.bonusScore = bonus;
        m_tableBonus += bonus;
    }
    if (bonus > 0) showBonusFloat(bonus);
    appendLog(QString("%1 出牌: %2 (%3%4)")
        .arg(selfName)
        .arg(cardsToString(chosen))
        .arg(cardTypeToQString(m_lastPlay.type))
        .arg(m_lastPlay.bonusScore > 0 ? QString(" 压分+%1").arg(m_lastPlay.bonusScore) : ""));

    if (m_lastPlay.bonusScore > 0 || m_lastPlay.type == CardType::Bomb || m_lastPlay.type == CardType::Rocket) {
        playSound(m_soundCasino);
    } else {
        playSound(m_soundCorrect);
    }

    updateUI();

    bool selfFinished = self->hand.empty();

    if (selfFinished && selfDeck->cards.empty()) {
        finishGame(*self, selfName, selfFinishSound);
        return;
    }

    if (opp->hand.empty() && !oppDeck->cards.empty()) {
        appendLog(selfName + " 出牌回应，" + selfName + " 赢得本回合");
        endRound(*self);
        refillBoth(*self, *opp);
        clearLastPlay();
        m_phase = GamePhase::AITurn;
        QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
        return;
    }

    startNextTurn();
    updateUI();
}

// ============================================================
// Round management helpers
// ============================================================

void BattleScreen::clearLastPlay()
{
    m_lastPlay.type = CardType::Invalid;
    m_lastPlay.cards.clear();
    m_lastPlay.keyPoint.clear();
    m_lastPlayerName.clear();
}

bool BattleScreen::handleSpecialVictoryCheck()
{
    if (checkSpecialVictory(m_playerA)) {
        appendLog("玩家A 达成七鬼523，直接获胜！");
        playSound(m_soundSuccess);
        m_pendingSpecialVictory = true;
        showSpecialVictoryEffect("玩家A", "玩家A 达成七鬼523，直接获胜！");
        return true;
    }
    if (checkSpecialVictory(m_playerB)) {
        appendLog("电脑 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showSpecialVictoryEffect("电脑", "电脑 达成七鬼523，直接获胜！");
        return true;
    }
    return false;
}

void BattleScreen::finishGame(Player& finisher, const QString& finisherName, QMediaPlayer* sound)
{
    Player& opponent = (&finisher == &m_playerA) ? m_playerB : m_playerA;
    endRound(finisher);
    finalSettlement(finisher, opponent, m_tableCards);
    compareAndAnnounce(m_playerA, m_playerB);

    appendLog("========== 游戏结束 ==========");
    appendLog(QString("出完牌者: %1").arg(finisherName));
    appendLog(QString("玩家A 总分: %1").arg(m_playerA.totalScore));
    appendLog(QString("电脑 总分: %1").arg(m_playerB.totalScore));
    QString w = (m_playerA.totalScore >= m_playerB.totalScore) ? "玩家A" : "电脑";
    appendLog(QString("最终胜者: %1").arg(w));

    playSound(sound);
    showGameOverDialog(QString("%1 出完牌！\n玩家A: %2 分\n电脑: %3 分")
        .arg(finisherName)
        .arg(m_playerA.totalScore)
        .arg(m_playerB.totalScore));
    disableActionButtons();
}

std::vector<Card> BattleScreen::dispatchAI(Player& self, Player& opp, Deck& selfDeck)
{
    int tableScore = calculateScore(m_tableCards);

    if (m_isLevelMode) {
        if (m_currentLevel == 1) {
            return ai1_idiot_choose(self, opp, m_lastPlay, selfDeck, tableScore);
        }
        if (m_currentLevel == 2) {
            return ai2_liar_choose(self, opp, m_lastPlay, selfDeck, tableScore);
        }
        if (m_currentLevel == 3) {
            return ai1_idiot_choose(self, opp, m_lastPlay, selfDeck, tableScore);
        }
        if (m_levelAIEngine) {
            m_levelAIEngine->setOpponentHand(opp.hand);
            return m_levelAIEngine->choosePlay(self, opp, m_lastPlay, selfDeck, tableScore);
        }
        return aiChoosePlay(self, opp, m_lastPlay, selfDeck, tableScore, m_tracker);
    }

    if (self.aiLevel == AILevel::AI2_Rule) {
        return ai2_liar_choose(self, opp, m_lastPlay, selfDeck, tableScore);
    }
    return ai1_idiot_choose(self, opp, m_lastPlay, selfDeck, tableScore);
}

void BattleScreen::startNewGame()
{
    VariantRegistry::clear();

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
        m_levelEngineConfig = buildAIEngineConfig(m_currentLevel);
        m_levelAIEngine = std::make_unique<AIEngine>(m_levelEngineConfig);
        m_levelAIEngine->setOpponentHand(m_playerA.hand);
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

void BattleScreen::endRound(Player& winner)
{
    if (m_isLevelMode && !m_playerPlayedCards.empty()) {
        for (auto& c : m_playerPlayedCards) {
            c.seq = m_playerNextSeq++;
            m_playerHiddenDeck.cards.push_back(c);
        }
        m_playerPlayedCards.clear();
        std::sort(m_playerHiddenDeck.cards.begin(), m_playerHiddenDeck.cards.end(),
            [](const Card& a, const Card& b) { return a.seq < b.seq; });
    }
    settleScoreCards(winner, m_tableCards);
    winner.totalScore += m_tableBonus;

    int score = calculateTableScore(m_tableCards, m_tableBonus);
    if (score > 0) {
        appendLog(QString("%1 获得 %2 分%3")
            .arg(QString::fromStdString(winner.name))
            .arg(score)
            .arg(m_tableBonus > 0 ? QString(" (压分奖励 %1)").arg(m_tableBonus) : ""));
    }

    if (m_isLevelMode && score > 0) {
        if (&winner == &m_playerA) {
            m_campaign.applyRoundDamage(score, 0);
            showHpDamageFloat(score, true);
        } else {
            m_campaign.applyRoundDamage(0, score);
            showHpDamageFloat(score, false);
        }
        updateLabels();
    }

    m_tableCards.clear();
    m_tableBonus = 0;
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    m_lastPlay.type = CardType::Invalid;
    m_lastPlay.cards.clear();
    m_lastPlay.keyPoint.clear();
    ++m_roundCount;
executeCheats(CheatWhen::RoundStart);
appendLog(QString("--- 回合 %1 结束 ---").arg(m_roundCount));

    playSound(m_soundShine);

    if (m_tableFrame) {
        QString savedStyle = m_tableFrame->styleSheet();
        m_tableFrame->setStyleSheet(R"(
            QFrame {
                background-color: #3a2020;
                border: 3px solid #FFFFFF;
                border-radius: 12px;
            }
        )");
        QTimer::singleShot(250, this, [this, savedStyle]() {
            if (m_tableFrame) {
                m_tableFrame->setStyleSheet(savedStyle);
            }
        });
    }

    updateUI();
}

QParallelAnimationGroup* BattleScreen::animateNewCardsToHand(
    const std::vector<QWidget*>& cards,
    QWidget* deckSource,
    QWidget* handWidget,
    bool playerA)
{
    if (cards.empty() || !deckSource || !handWidget) {
        layoutHandSlots(playerA, false);
        return nullptr;
    }

    QPoint deckPos = deckSource->mapTo(this, QPoint(0, 0));
    QPoint deckCenter(deckPos.x() + deckSource->width() / 2,
                      deckPos.y() + deckSource->height() / 2);

    auto* group = new QParallelAnimationGroup(this);

    for (QWidget* w : cards) {
        if (!w) continue;

        QPoint finalPosInHand = w->pos();
        QPoint startPos(deckCenter.x() - w->width() / 2,
                        deckCenter.y() - w->height() / 2);
        QPoint finalPosInThis = handWidget->mapTo(this, finalPosInHand);

        w->setParent(this);
        w->raise();
        w->show();
        w->move(startPos);

        auto* anim = new QPropertyAnimation(w, "pos");
        anim->setDuration(450);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(startPos);
        anim->setEndValue(finalPosInThis);
        group->addAnimation(anim);
    }

    connect(group, &QParallelAnimationGroup::finished, this,
        [this, cards, handWidget, playerA]() {
            for (QWidget* w : cards) {
                if (!w) continue;
                w->setParent(handWidget);
                w->show();
            }
            layoutHandSlots(playerA, false);
        });

    group->start(QAbstractAnimation::DeleteWhenStopped);
    return group;
}

void BattleScreen::refillBoth(Player& winner, Player& loser)
{
    // 保存补牌前的手牌（用于识别新增牌）
    std::vector<Card> oldPlayerAHand = m_playerA.hand;
    std::vector<Card> oldPlayerBHand = m_playerB.hand;

    // 双牌堆：赢家和输家各自从自己的牌堆补牌
    Deck& winnerDeck = (winner.name == m_playerA.name) ? m_playerDeck : m_bossDeck;
    Deck& loserDeck  = (loser.name  == m_playerA.name) ? m_playerDeck : m_bossDeck;

    bool winnerIsPlayerA = (winner.name == m_playerA.name);
    bool loserIsPlayerA = (loser.name == m_playerA.name);

    int beforeW = static_cast<int>(winner.hand.size());
    int beforeL = static_cast<int>(loser.hand.size());

    refillToFive(winner, winnerDeck);
    refillToFive(loser, loserDeck);

    int gotW = static_cast<int>(winner.hand.size()) - beforeW;
    int gotL = static_cast<int>(loser.hand.size()) - beforeL;

    qDebug() << "[refillBoth-ENTER] phase=" << static_cast<int>(m_phase)
             << " winner=" << QString::fromStdString(winner.name)
             << "loser=" << QString::fromStdString(loser.name)
             << "winnerIsPlayerA=" << winnerIsPlayerA
             << "beforeW=" << beforeW << "beforeL=" << beforeL
             << "gotW=" << gotW << "gotL=" << gotL;

    appendLog(QString("补牌: %1 +%2张 → %3张, %4 +%5张 → %6张")
        .arg(QString::fromStdString(winner.name)).arg(gotW).arg(static_cast<int>(winner.hand.size()))
        .arg(QString::fromStdString(loser.name)).arg(gotL).arg(static_cast<int>(loser.hand.size())));

    sortHandSmart(m_playerA.hand);

    qDebug() << "[refillBoth] after sort, playerA hand size=" << static_cast<int>(m_playerA.hand.size());
    for (size_t i = 0; i < m_playerA.hand.size(); ++i) {
        qDebug() << "  hand[" << i << "]=" << QString::fromStdString(m_playerA.hand[i].point)
                 << QString::fromStdString(m_playerA.hand[i].suit)
                 << "seq=" << m_playerA.hand[i].seq;
    }

    m_handDirty = true;
    // updateUI(true, true) 重建手牌 widget，新牌隐藏（防止闪现）
    updateUI(true, true);

    qDebug() << "[refillBoth] after updateUI, playerAWidgets="
             << (int)m_playerACardWidgets.size()
             << "playerBWidgets=" << (int)m_playerBCardWidgets.size();

    // 对玩家A的新增牌：找到 widget → 移到牌堆起点 → 动画飞回最终位置
    if (gotW > 0 && winnerIsPlayerA) {
        qDebug() << "[refillBoth] playerA-winner branch gotW=" << gotW;
        std::vector<int> newIndices;
        for (int i = 0; i < static_cast<int>(m_playerA.hand.size()); ++i) {
            const Card& c = m_playerA.hand[i];
            bool found = false;
            for (const Card& old : oldPlayerAHand) {
                if (c.point == old.point && c.suit == old.suit) {
                    found = true;
                    break;
                }
            }
            if (!found)
                newIndices.push_back(i);
        }

        std::vector<QWidget*> newWidgets;
        for (int idx : newIndices) {
            if (idx < static_cast<int>(m_playerACardWidgets.size()))
                newWidgets.push_back(m_playerACardWidgets[idx]);
        }

        if (!newWidgets.empty()) {
            m_phase = GamePhase::RefillAnimation;
            qDebug() << "[refillBoth] animateNewCardsToHand count=" << static_cast<int>(newWidgets.size());
            animateNewCardsToHand(newWidgets, m_playerDeckWidget, m_playerAHandWidget, true);
        }
    } else if (gotW > 0 || gotL > 0) {
        qDebug() << "[refillBoth] else-if branch gotW=" << gotW << "gotL=" << gotL;

        // 玩家A侧新牌
        std::vector<QWidget*> aNewWidgets;
        if (gotL > 0 && loserIsPlayerA) {
            for (int i = 0; i < static_cast<int>(m_playerA.hand.size()); ++i) {
                const Card& c = m_playerA.hand[i];
                bool found = false;
                for (const Card& old : oldPlayerAHand) {
                    if (c.point == old.point && c.suit == old.suit) {
                        found = true;
                        break;
                    }
                }
                if (!found && i < static_cast<int>(m_playerACardWidgets.size()))
                    aNewWidgets.push_back(m_playerACardWidgets[i]);
            }
        }
        if (!aNewWidgets.empty())
            animateNewCardsToHand(aNewWidgets, m_playerDeckWidget, m_playerAHandWidget, true);

        // Boss侧新牌
        std::vector<QWidget*> bNewWidgets;
        if (gotW > 0 && !winnerIsPlayerA) {
            for (int i = 0; i < gotW; ++i) {
                int idx = beforeW + i;
                if (idx < static_cast<int>(m_playerBCardWidgets.size()))
                    bNewWidgets.push_back(m_playerBCardWidgets[idx]);
            }
        }
        if (!bNewWidgets.empty())
            animateNewCardsToHand(bNewWidgets, m_bossDeckWidget, m_playerBHandWidget, false);
    }

    qDebug() << "[refillBoth] exit, widgets=" << static_cast<int>(m_playerACardWidgets.size());

    // Stress test: verify widgets after refill
    if (m_stressPlaying) {
        QTimer::singleShot(kRefillCheckStressMs, this, [this]() {
            if (m_phase == GamePhase::RefillAnimation) return;
            bool ok = true;
            if (m_playerACardWidgets.size() != m_playerA.hand.size()) {
                qDebug() << "[STRESS] REFILL FAIL: A widgets="
                         << static_cast<int>(m_playerACardWidgets.size())
                         << "hand=" << static_cast<int>(m_playerA.hand.size());
                ok = false;
            }
            if (m_playerBCardWidgets.size() != m_playerB.hand.size()) {
                qDebug() << "[STRESS] REFILL FAIL: B widgets="
                         << static_cast<int>(m_playerBCardWidgets.size())
                         << "hand=" << static_cast<int>(m_playerB.hand.size());
                ok = false;
            }
            for (size_t i = 0; i < m_playerACardWidgets.size(); ++i) {
                auto* cw = m_playerACardWidgets[i];
                if (!cw || !cw->isVisible()
                    || cw->parentWidget() != m_playerAHandWidget) {
                    qDebug() << "[STRESS] REFILL FAIL: widget" << i
                             << "invalid (vis=" << (cw ? cw->isVisible() : false)
                             << "parentOK=" << (cw && cw->parentWidget() == m_playerAHandWidget)
                             << ")";
                    ok = false;
                    break;
                }
            }
            if (ok) {
                qDebug() << "[STRESS] REFILL OK";
            }
        });
    }
}

void BattleScreen::runAutoPlayTest(int totalGames)
{
    m_stressTotalGames = totalGames;
    m_stressGamesLeft = totalGames;
    m_stressSuccessGames = 0;
    m_stressFailedGames = 0;
    m_stressPlaying = true;
    m_aiTurnDelayMs = kAITurnDelayStressMs;

    qDebug() << "[STRESS] ============ START ============";
    qDebug() << "[STRESS] total games=" << totalGames;

    autoPlayOneGame();
}

void BattleScreen::autoPlayOneGame()
{
    if (m_stressGamesLeft <= 0) {
        qDebug() << "[STRESS] ============ DONE ============";
        qDebug() << "[STRESS] total=" << m_stressTotalGames
                 << "success=" << m_stressSuccessGames
                 << "failed=" << m_stressFailedGames;
        m_stressPlaying = false;
        emit autoPlayTestFinished();
        return;
    }

    m_stressGamesLeft--;
    m_stressDoAICallCount   = 0;
    m_stressNoProgressCount = 0;
    m_lastTurnSignature     = {-1,-1,-1,-1};

    int gameNum = m_stressTotalGames - m_stressGamesLeft;

    m_isLevelMode = false;
    m_currentLevel = 0;
    m_playerAIsFirst = false;
    m_isStressPlayerATurn = false;

    startNewGame();

    m_playerA.isHuman = false;
    m_playerA.aiLevel = AILevel::AI1_Simple;
    m_playerB.aiLevel = AILevel::AI1_Simple;

    qDebug() << "[STRESS] game" << gameNum << "dealing...";

    startGameWithFirst(false);
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