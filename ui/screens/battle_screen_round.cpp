#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"
#include "game/campaign.h"
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QDebug>
#include <algorithm>
#include <memory>

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
    m_activeRefillGroups.push_back(group);

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

    std::vector<QPointer<QWidget>> cardsGuard;
    for (QWidget* w : cards) cardsGuard.push_back(QPointer<QWidget>(w));

    connect(group, &QParallelAnimationGroup::finished, this,
        [this, cardsGuard, cards, handWidget, playerA, group]() {
            for (const QPointer<QWidget>& wGuard : cardsGuard) {
                if (wGuard.isNull()) continue;
                wGuard->setParent(handWidget);
                wGuard->show();
            }
            layoutHandSlots(playerA, false);
            auto it = std::find(m_activeRefillGroups.begin(), m_activeRefillGroups.end(), group);
            if (it != m_activeRefillGroups.end()) {
                m_activeRefillGroups.erase(it);
            }
            group->deleteLater();
        });

    bool allValid = true;
    for (int i = 0; i < group->animationCount(); ++i) {
        auto* propAnim = qobject_cast<QPropertyAnimation*>(group->animationAt(i));
        if (!propAnim || !propAnim->targetObject()) {
            qDebug() << "[animateNewCardsToHand] WARN: animation target is null, aborting group";
            allValid = false;
            break;
        }
    }
    if (!allValid) {
        delete group;
        auto it = std::find(m_activeRefillGroups.begin(), m_activeRefillGroups.end(), group);
        if (it != m_activeRefillGroups.end()) {
            m_activeRefillGroups.erase(it);
        }
        return nullptr;
    }

    group->start(QAbstractAnimation::KeepWhenStopped);
    return group;
}

void BattleScreen::refillBoth(Player& winner, Player& loser)
{
    std::vector<Card> oldPlayerAHand = m_playerA.hand;
    std::vector<Card> oldPlayerBHand = m_playerB.hand;

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
    updateUI(true, true);

    qDebug() << "[refillBoth] after updateUI, playerAWidgets="
             << (int)m_playerACardWidgets.size()
             << "playerBWidgets=" << (int)m_playerBCardWidgets.size();

    QParallelAnimationGroup* groupA  = nullptr;
    QParallelAnimationGroup* groupA2 = nullptr;
    QParallelAnimationGroup* groupB  = nullptr;

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
            groupA = animateNewCardsToHand(newWidgets, m_playerDeckWidget, m_playerAHandWidget, true);
        }
    } else if (gotW > 0 || gotL > 0) {
        qDebug() << "[refillBoth] else-if branch gotW=" << gotW << "gotL=" << gotL;

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
        if (!aNewWidgets.empty()) {
            groupA2 = animateNewCardsToHand(aNewWidgets, m_playerDeckWidget, m_playerAHandWidget, true);
        }

        std::vector<QWidget*> bNewWidgets;
        if (gotW > 0 && !winnerIsPlayerA) {
            for (int i = 0; i < gotW; ++i) {
                int idx = beforeW + i;
                if (idx < static_cast<int>(m_playerBCardWidgets.size()))
                    bNewWidgets.push_back(m_playerBCardWidgets[idx]);
            }
        }
        if (!bNewWidgets.empty()) {
            groupB = animateNewCardsToHand(bNewWidgets, m_bossDeckWidget, m_playerBHandWidget, false);
        }
    }

    qDebug() << "[refillBoth] exit, widgets=" << static_cast<int>(m_playerACardWidgets.size());

    if (m_stressPlaying) {
        auto doRefillCheck = [this]() {
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
        };

        std::vector<QParallelAnimationGroup*> verifyGroups;
        if (groupA)  verifyGroups.push_back(groupA);
        if (groupA2) verifyGroups.push_back(groupA2);
        if (groupB)  verifyGroups.push_back(groupB);

        if (verifyGroups.empty()) {
            QTimer::singleShot(100, this, doRefillCheck);
        } else {
            auto pending = std::make_shared<int>(verifyGroups.size());
            for (auto* g : verifyGroups) {
                connect(g, &QParallelAnimationGroup::finished, this,
                    [this, pending, doRefillCheck]() {
                        if (--(*pending) == 0) {
                            QTimer::singleShot(50, this, doRefillCheck);
                        }
                    });
            }
        }
    }
}