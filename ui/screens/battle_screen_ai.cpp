#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ai/ai.h"
#include "ai/players/ai1_idiot.h"
#include "ai/players/ai2_liar.h"
#include "ai/engine/ai_levels.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <QDebug>
#include <QTimer>
#include <algorithm>

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