#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/dialogs/card_picker.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
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

    bool canPass = canBeat(result, m_lastPlay);

    if (m_lastPlay.type != CardType::Invalid && !canPass) {
        QMessageBox::warning(this, "无法压过", "你选的牌无法压过上一手");
        playSound(m_soundWrong);
        return;
    }

    if (m_isLevelMode && m_currentLevel == 5 && result.keyPoint == m_lastPlay.keyPoint) {
        QMessageBox::warning(this, "无法压过", "赖账鬼不让你用一样的点数压！");
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
            m_difficultyButton->setText("难度: 贪心");
            break;
        case AILevel::AI2_Rule:
            m_aiLevel = AILevel::AI4_Expert;
            m_difficultyButton->setText("难度: 搜索");
            break;
        case AILevel::AI4_Expert:
            m_aiLevel = AILevel::AI1_Simple;
            m_difficultyButton->setText("难度: 保守");
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