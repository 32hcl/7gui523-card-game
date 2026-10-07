#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/play_effect_overlay.h"
#include "ui/widgets/dialogue_overlay.h"
#include "ui/audio/audio_manager.h"
#include "dialogue/dialogue_engine.h"
#include "ai/ai.h"
#include "ai/players/ai1_idiot.h"
#include "game/level_boss.h"
#include "ai/ai_levels.h"
#include "core/card/cardtype.h"
#include "core/card/rank.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
#include <algorithm>

void BattleScreen::doAITurn()
{
    if (m_phase == GamePhase::GameOver) return;

    // 防御：非压力测试模式下，doAITurn只能在AITurn阶段被调用
    // 防止延迟定时器在PlayerTurn阶段触发导致回合被错误结束
    if (!m_stressPlaying && m_phase != GamePhase::AITurn) {
        qDebug() << "[doAITurn] 忽略非AI阶段的调用, phase =" << (int)m_phase;
        return;
    }

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

    if (m_stressPlaying && m_isStressPlayerATurn) {
        self = &m_playerA;  opp = &m_playerB;
        selfDeck = &m_playerDeck;  oppDeck = &m_bossDeck;
        selfName = "玩家A(AI)";  oppName = "电脑";
        selfSide = DeckSide::PlayerA;
    } else {
        self = &m_playerB;  opp = &m_playerA;
        selfDeck = &m_bossDeck;  oppDeck = &m_playerDeck;
        selfName = "电脑";  oppName = "玩家A";
        selfSide = DeckSide::Boss;
    }

    // 维护搜索模型视角字段（否则 AI3/AI4 终局判断恒真，模型失真）
    selfDeck->myRemaining = static_cast<int>(selfDeck->cards.size());
    oppDeck->oppRemaining = static_cast<int>(oppDeck->cards.size());

    std::vector<Card> chosen = dispatchAI(*self, *opp, *selfDeck);

    // ── Boss特殊能力（仅Pass时触发）──────
    if (chosen.empty() && m_isLevelMode && m_lastPlay.type != CardType::Invalid &&
        m_lastPlayerName != selfName && m_levelRule.tryForcePlay) {
        std::mt19937 tmpRng(QRandomGenerator::global()->generate());
        chosen = m_levelRule.tryForcePlay(*self, m_lastPlay, tmpRng);
        if (!chosen.empty() && m_levelRule.forcePlayMsg)
            appendLog(QString::fromUtf8(m_levelRule.forcePlayMsg));
    }

    // ── 没有牌可出 → 不要 → 回合结束 ──
    if (chosen.empty()) {
        appendLog(selfName + " 不要");

        endRound(*opp);
        refillBoth(*opp, *self);

        startNextTurn();
        updateUI(false);
        return;
    }

    for (const Card& c : chosen) {
        auto it = self->hand.end();
        if (c.seq > 0) {
            it = std::find_if(self->hand.begin(), self->hand.end(),
                [&](const Card& h) {
                    return h.seq == c.seq;
                });
        }
        if (it == self->hand.end()) {
            it = std::find_if(self->hand.begin(), self->hand.end(),
                [&](const Card& h) {
                    return h.point == c.point && h.suit == c.suit;
                });
        }
        if (it != self->hand.end()) self->hand.erase(it);
    }
    m_handDirty = true;
    for (const Card& c : chosen) {
        m_tableCards.push_back(c);
    }
    CardTypeResult oldLastPlay = m_lastPlay;
    m_lastPlay = parseAnyCardType(chosen);
    m_lastPlayerName = selfName;

    if (m_lastPlay.type == CardType::Bomb || m_lastPlay.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
        if (m_effectOverlay) {
            if (m_lastPlay.type == CardType::Rocket) {
                m_effectOverlay->showRocketEffect();
            } else {
                m_effectOverlay->showBombEffect();
            }
        }
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
        AUDIO_MGR.playSfx(AudioManager::Sfx::Special523);
        if (m_effectOverlay) {
            m_effectOverlay->showSpecial523Effect();
        }
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

    if (m_lastPlay.type == CardType::Bomb) {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Bomb);
    } else if (m_lastPlay.type == CardType::Rocket) {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Rocket);
    } else if (m_lastPlay.bonusScore > 0) {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Lose);
    } else {
        AUDIO_MGR.playSfx(AudioManager::Sfx::PlayCard);
    }

    if (m_isLevelMode && m_levelRule.bossDamage) {
        int bossDmg = m_levelRule.bossDamage(chosen);
        if (bossDmg > 0) {
            m_campaign.applyRoundDamage(0, bossDmg);
            showHpDamageFloat(bossDmg, false);
            if (m_levelRule.bossDamageMsg)
                appendLog(QString::fromUtf8(m_levelRule.bossDamageMsg).arg(bossDmg).arg((int)chosen.size()));
        }
    }

    updateUI();

    bool selfFinished = self->hand.empty();

    if (selfFinished && m_isLevelMode && m_levelRule.onBossFinish && self == &m_playerB) {
        std::mt19937 tmpRng(QRandomGenerator::global()->generate());
        auto discarded = m_levelRule.onBossFinish(m_playerA.hand, m_playerDeck.cards, tmpRng);
        if (!discarded.empty() && m_levelRule.onBossFinishMsg) {
            appendLog(QString::fromUtf8(m_levelRule.onBossFinishMsg)
                         .arg(cardsToString(discarded)));
            m_handDirty = true;
        }
    }

    if (selfFinished && selfDeck->cards.empty()) {
        finishGame(*self, selfName);
        return;
    }

    // ── 关卡模式：Boss出特殊牌时的对话 ──
    // 只在Boss回合（非压力测试）且是正常出牌时触发
    if (m_isLevelMode && !m_stressPlaying && self == &m_playerB
        && m_dialogueOverlay && !m_dialogueOverlay->isPlaying()) {
        bool hasSpecial = false;
        DialogueTrigger trigger = DialogueTrigger::PreBattle;

        if (m_lastPlay.type == CardType::Bomb && !m_bossFirstBombShown) {
            trigger = DialogueTrigger::OnFirstBomb;
            m_bossFirstBombShown = true;
            hasSpecial = true;
        } else if (m_lastPlay.type == CardType::Rocket && !m_bossFirstRocketShown) {
            trigger = DialogueTrigger::OnFirstRocket;
            m_bossFirstRocketShown = true;
            hasSpecial = true;
        }

        if (hasSpecial) {
            auto lines = DialogueEngine::instance().getDialogue(m_currentLevel, trigger);
            if (!lines.isEmpty()) {
                m_dialogueCtx = DialogueCtx::BossSpecial;
                m_dialogueOverlay->playDialogue(lines, m_currentLevel);
                updateUI();
                return;  // 对话结束后由 onDialogueFinished 进入玩家回合
            }
        }
    }

    m_phase = GamePhase::PlayerTurn;
    enableActionButtons();
    updateUI();
}

std::vector<Card> BattleScreen::dispatchAI(Player& self, Player& opp, Deck& selfDeck)
{
    int tableScore = calculateScore(m_tableCards);

    if (m_isLevelMode) {
        if (m_currentLevel == 1 || m_currentLevel == 7 || m_currentLevel == 9) {
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

    return aiChoosePlay(self, opp, m_lastPlay, selfDeck, tableScore, m_tracker);
}