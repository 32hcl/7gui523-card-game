#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/dialogs/card_picker.h"
#include "ai/ai.h"
#include "ai/engine/ai_levels.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"
#include "game/campaign.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDialog>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <random>

void BattleScreen::onPlayButtonClicked()
{
    if (m_waitingForAI || m_gameOver) return;

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
        m_playerALayout->removeWidget(cw);
        m_playerALayout->update();
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

    if (result.type == CardType::Bomb || result.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
    }

    for (const Card& c : selected) {
        m_tableCards.push_back(c);
    }
    CardTypeResult oldLastPlay = m_lastPlay;
    m_lastPlay = result;
    m_lastPlayerName = "玩家A";

    m_tracker.recordPlayed(selected);

    if (result.type == CardType::Special523) {
        appendLog("玩家A 达成七鬼523，直接获胜！");
        playSound(m_soundSuccess);
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
        endRound(m_playerA);
        finalSettlement(m_playerA, m_playerB, m_tableCards);
        compareAndAnnounce(m_playerA, m_playerB);

        appendLog("========== 游戏结束 ==========");
        appendLog("出完牌者: 玩家A");
        appendLog(QString("玩家A 总分: %1").arg(m_playerA.totalScore));
        appendLog(QString("电脑 总分: %1").arg(m_playerB.totalScore));
        QString w = (m_playerA.totalScore >= m_playerB.totalScore) ? "玩家A" : "电脑";
        appendLog(QString("最终胜者: %1").arg(w));

        playSound(m_soundSuccess);
        showGameOverDialog(QString("玩家A 出完牌！\n玩家A: %1 分\n电脑: %2 分")
            .arg(m_playerA.totalScore).arg(m_playerB.totalScore));
        disableActionButtons();
        return;
    }

    if (m_playerB.hand.empty() && !m_bossDeck.cards.empty()) {
        appendLog("玩家A 出牌回应，玩家A 赢得本回合");
        endRound(m_playerA);
        refillBoth(m_playerA, m_playerB);
        m_lastPlay.type = CardType::Invalid;
        m_lastPlay.cards.clear();
        m_lastPlay.keyPoint.clear();
        m_lastPlayerName.clear();
        updateUI();
        enableActionButtons();
        return;
    }

    m_waitingForAI = true;
    QTimer::singleShot(700, this, &BattleScreen::doAITurn);
    if (playerAFinished) {
        m_playButton->setEnabled(false);
    }
}

void BattleScreen::onPassButtonClicked()
{
    playSound(m_soundClick);
    if (m_waitingForAI || m_gameOver) return;

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
        m_lastPlay.type = CardType::Invalid;
        m_lastPlay.cards.clear();
        m_lastPlay.keyPoint.clear();
        m_lastPlayerName.clear();
        updateUI();

        if (checkSpecialVictory(m_playerA)) {
            appendLog("玩家A 达成七鬼523，直接获胜！");
            playSound(m_soundSuccess);
            showSpecialVictoryEffect("玩家A", "玩家A 达成七鬼523，直接获胜！");
            return;
        }
        if (checkSpecialVictory(m_playerB)) {
            appendLog("电脑 达成七鬼523，直接获胜！");
            playSound(m_soundFailure);
            showSpecialVictoryEffect("电脑", "电脑 达成七鬼523，直接获胜！");
            return;
        }

        m_waitingForAI = true;
        QTimer::singleShot(700, this, &BattleScreen::doAITurn);
        return;
    }

    endRound(m_playerB);

    if (!m_deck.cards.empty()) {
        refillBoth(m_playerB, m_playerA);
    }

    updateUI();

    if (checkSpecialVictory(m_playerA)) {
        appendLog("玩家A 达成七鬼523，直接获胜！");
        playSound(m_soundSuccess);
        showSpecialVictoryEffect("玩家A", "玩家A 达成七鬼523，直接获胜！");
        return;
    }
    if (checkSpecialVictory(m_playerB)) {
        appendLog("电脑 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showSpecialVictoryEffect("电脑", "电脑 达成七鬼523，直接获胜！");
        return;
    }

    m_lastPlay.type = CardType::Invalid;
    m_lastPlay.cards.clear();
    m_lastPlay.keyPoint.clear();
    m_lastPlayerName.clear();
    m_waitingForAI = true;
    QTimer::singleShot(700, this, &BattleScreen::doAITurn);
}

void BattleScreen::onNewGameButtonClicked()
{
    playSound(m_soundClick);
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    m_gameOver = false;
    startNewGame();
}

void BattleScreen::onDifficultyButtonClicked()
{
    playSound(m_soundClick);
    switch (m_aiLevel) {
        case AILevel::AI1_Simple:
            m_aiLevel = AILevel::AI2_Rule;
            m_difficultyButton->setText("难度: 电脑2");
            break;
        case AILevel::AI2_Rule:
            m_aiLevel = AILevel::AI3_Tracker;
            m_difficultyButton->setText("难度: 电脑3");
            break;
        case AILevel::AI3_Tracker:
            m_aiLevel = AILevel::AI4_Expert;
            m_difficultyButton->setText("难度: 电脑4");
            break;
        case AILevel::AI4_Expert:
            m_aiLevel = AILevel::AI1_Simple;
            m_difficultyButton->setText("难度: 电脑1");
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
    int needA = 5 - (int)m_playerA.hand.size();
    if (needA > 0) dealCards(m_playerA, m_playerDeck, needA);
    sortHandSmart(m_playerA.hand);

    dealCards(m_playerB, m_bossDeck, 5);

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
    m_dealAnimating = false;
    m_difficultyButton->setEnabled(true);
    m_newGameButton->setEnabled(true);
    m_buttonStack->setEnabled(true);
    m_buttonStack->setCurrentIndex(0);

    if (m_playerAIsFirst) {
        m_waitingForAI = false;
        enableActionButtons();
    } else {
        m_waitingForAI = true;
        disableActionButtons();
        QTimer::singleShot(300, this, &BattleScreen::doAITurn);
    }
    updateUI(false);
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
    m_waitingForAI = false;
    if (m_gameOver) return;

    int tableScore = calculateScore(m_tableCards);
    std::vector<Card> chosen;
    if (m_isLevelMode && m_levelAIEngine) {
        m_levelAIEngine->setOpponentHand(m_playerA.hand);
        chosen = m_levelAIEngine->choosePlay(
            m_playerB, m_playerA, m_lastPlay, m_bossDeck, tableScore);
    } else {
        chosen = aiChoosePlay(
            m_playerB, m_playerA, m_lastPlay, m_bossDeck, tableScore, m_tracker);
    }

    if (chosen.empty()) {
        appendLog("电脑 不要");

        if (m_playerA.hand.empty() && !m_playerDeck.cards.empty()) {
            appendLog("玩家A 赢得本回合");
            endRound(m_playerA);
            refillBoth(m_playerA, m_playerB);
            m_lastPlay.type = CardType::Invalid;
            m_lastPlay.cards.clear();
            m_lastPlay.keyPoint.clear();
            m_lastPlayerName.clear();
            updateUI();
            enableActionButtons();
            return;
        }

        endRound(m_playerA);

        if (!m_playerDeck.cards.empty()) {
            refillBoth(m_playerA, m_playerB);
        }

        updateUI();

        if (checkSpecialVictory(m_playerA)) {
            appendLog("玩家A 达成七鬼523，直接获胜！");
            playSound(m_soundSuccess);
            showSpecialVictoryEffect("玩家A", "玩家A 达成七鬼523，直接获胜！");
            return;
        }
        if (checkSpecialVictory(m_playerB)) {
            appendLog("电脑 达成七鬼523，直接获胜！");
            playSound(m_soundFailure);
            showSpecialVictoryEffect("电脑", "电脑 达成七鬼523，直接获胜！");
            return;
        }

        m_lastPlay.type = CardType::Invalid;
        m_lastPlay.cards.clear();
        m_lastPlay.keyPoint.clear();
        m_lastPlayerName.clear();
        enableActionButtons();
        updateUI();
        return;
    }

    for (const Card& c : chosen) {
        auto it = std::find_if(m_playerB.hand.begin(), m_playerB.hand.end(),
            [&](const Card& h) {
                return h.point == c.point && h.suit == c.suit;
            });
        if (it != m_playerB.hand.end()) m_playerB.hand.erase(it);
    }
    for (const Card& c : chosen) {
        m_tableCards.push_back(c);
    }
    CardTypeResult oldLastPlay = m_lastPlay;
    m_lastPlay = parseCardType(chosen);
    m_lastPlayerName = "电脑";

    if (m_lastPlay.type == CardType::Bomb || m_lastPlay.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
    }

    if (m_levelAIEngine) m_levelAIEngine->recordPlayed(chosen);
    m_tracker.recordPlayed(chosen);

    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    flyAICardsToTable(chosen);

    if (m_lastPlay.type == CardType::Special523) {
        appendLog("电脑 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showSpecialVictoryEffect("电脑", "电脑 达成七鬼523，直接获胜！");
        return;
    }

    int bonus = calculatePressureBonus(m_lastPlay, oldLastPlay);
    if (bonus > 0) {
        m_lastPlay.bonusScore = bonus;
        m_tableBonus += bonus;
    }
    if (bonus > 0) showBonusFloat(bonus);
    appendLog(QString("电脑 出牌: %1 (%2%3)")
        .arg(cardsToString(chosen))
        .arg(cardTypeToQString(m_lastPlay.type))
        .arg(m_lastPlay.bonusScore > 0 ? QString(" 压分+%1").arg(m_lastPlay.bonusScore) : ""));

    if (m_lastPlay.bonusScore > 0 || m_lastPlay.type == CardType::Bomb || m_lastPlay.type == CardType::Rocket) {
        playSound(m_soundCasino);
    } else {
        playSound(m_soundCorrect);
    }

    updateUI();

    bool playerBFinished = m_playerB.hand.empty();

    if (playerBFinished && m_bossDeck.cards.empty()) {
        endRound(m_playerB);
        finalSettlement(m_playerB, m_playerA, m_tableCards);
        compareAndAnnounce(m_playerA, m_playerB);

        appendLog("========== 游戏结束 ==========");
        appendLog("出完牌者: 电脑");
        appendLog(QString("玩家A 总分: %1").arg(m_playerA.totalScore));
        appendLog(QString("电脑 总分: %1").arg(m_playerB.totalScore));
        QString w = (m_playerA.totalScore >= m_playerB.totalScore) ? "玩家A" : "电脑";
        appendLog(QString("最终胜者: %1").arg(w));

        playSound(m_soundFailure);
        showGameOverDialog(QString("电脑 出完牌！\n玩家A: %1 分\n电脑: %2 分")
            .arg(m_playerA.totalScore).arg(m_playerB.totalScore));
        disableActionButtons();
        return;
    }

    if (m_playerA.hand.empty() && !m_playerDeck.cards.empty()) {
        appendLog("电脑 出牌回应，电脑 赢得本回合");
        endRound(m_playerB);
        refillBoth(m_playerB, m_playerA);
        m_lastPlay.type = CardType::Invalid;
        m_lastPlay.cards.clear();
        m_lastPlay.keyPoint.clear();
        m_lastPlayerName.clear();
        updateUI();
        m_waitingForAI = true;
        QTimer::singleShot(700, this, &BattleScreen::doAITurn);
        return;
    }

    enableActionButtons();
    updateUI();
}

void BattleScreen::startNewGame()
{
    for (CardWidget* cw : m_tableCardWidgets) cw->deleteLater();
    m_tableCardWidgets.clear();

    {
        auto children = m_tableCardsWidget->findChildren<QLabel*>();
        for (QLabel* lbl : children) delete lbl;
    }

    m_gameOver = false;
    m_waitingForAI = true;
    m_dealAnimating = false;
    m_pendingPick = true;
    m_lastPlay = CardTypeResult{};
    m_tableCards.clear();
    m_tableBonus = 0;
    m_lastPlayerName.clear();
    m_playerACardWidgets.clear();
    m_pickedCards.clear();

    m_deck = createStandardDeck();
    shuffleDeck(m_deck);

    m_playerA = createPlayer("玩家A");
    m_playerB = createPlayer("电脑");
    m_playerA.isHuman = true;

    if (m_isLevelMode) {
        int aiLevelNum = getLevelAILevel(m_currentLevel);
        m_levelEngineConfig = buildAIEngineConfig(aiLevelNum);
        m_levelAIEngine = std::make_unique<AIEngine>(m_levelEngineConfig);
        m_levelAIEngine->setOpponentHand(m_playerA.hand);
        m_playerB.aiLevel = AILevel::AI4_Expert;
        m_tracker.reset();
        m_roundCount = 1;

        m_logTextEdit->clear();
        appendLog(QString("========== 关卡模式 - 第 %1 关 ==========").arg(m_currentLevel));
        appendLog(QString("对手: %1").arg(getLevelDisplayName(m_currentLevel)));
        appendLog("请选择先手方...");

        // 显示先后手选择（与练习模式相同）
        if (m_firstChoiceWidget) {
            m_firstChoiceWidget->setGeometry(m_tableCardsWidget->rect());
            m_firstChoiceWidget->setVisible(true);
            m_firstChoiceWidget->raise();
            m_waitingForFirstChoice = true;
        }

        m_pendingPick = false;

        m_pickedCards.clear();

        // 双牌堆：玩家标准牌组，Boss 按关卡剔牌
        {
            auto playerCards = removeCards(m_deck.cards, {});
            m_playerDeck.cards = drawRandom(playerCards, 27);
            auto bossCards = removeCards(m_deck.cards, getLevelRemoveTable(m_currentLevel));
            m_bossDeck.cards = drawRandom(bossCards, 27);
        }

        dealCards(m_playerA, m_playerDeck, 5);
        sortHandSmart(m_playerA.hand);
        dealCards(m_playerB, m_bossDeck, 5);

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

    // 练习模式也使用双牌堆（标准牌组，各27张）
    {
        auto playerCards = removeCards(m_deck.cards, {});
        m_playerDeck.cards = drawRandom(playerCards, 27);
        auto bossCards = removeCards(m_deck.cards, {});
        m_bossDeck.cards = drawRandom(bossCards, 27);
    }

    m_difficultyButton->setVisible(true);
    m_difficultyButton->setEnabled(true);

    m_pendingPick = false;

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
    m_buttonStack->setCurrentIndex(1);

    updateUI();
    emit gameStarted();
}

void BattleScreen::updateUI(bool rebuildHand)
{
    // 双牌堆显示
    int playerDeckCount = static_cast<int>(m_playerDeck.cards.size());
    int bossDeckCount = static_cast<int>(m_bossDeck.cards.size());
    m_deckCountLabel->setText(
        QString("玩家牌堆: %1 | Boss牌堆: %2").arg(playerDeckCount).arg(bossDeckCount));
    if (m_deckCountBigLabel) {
        m_deckCountBigLabel->setText(
            QString("%1 / %2").arg(playerDeckCount).arg(bossDeckCount));
    }
    // 更新双牌堆 UI
    if (m_playerDeckCountLabel) {
        m_playerDeckCountLabel->setText(QString("%1 张").arg(playerDeckCount));
    }
    if (m_bossDeckCountLabel) {
        m_bossDeckCountLabel->setText(QString("%1 张").arg(bossDeckCount));
    }
    m_roundLabel->setText(
        QString("回合: %1").arg(m_roundCount));

    m_scoreALabel->setText(
        QString("玩家A: %1 分").arg(m_playerA.totalScore));
    m_scoreBLabel->setText(
        QString("电脑: %1 分").arg(m_playerB.totalScore));

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

    if (!m_waitingForFirstChoice && !m_pendingPick && m_tableCardWidgets.empty()) {
        if (m_lastPlay.type == CardType::Invalid || m_lastPlay.cards.empty()) {
            auto children = m_tableCardsWidget->findChildren<QLabel*>();
            for (QLabel* lbl : children) delete lbl;
            QLabel* hint = new QLabel("等待出牌", m_tableCardsWidget);
            hint->setAlignment(Qt::AlignCenter);
            hint->setGeometry(0, 0, m_tableCardsWidget->width(), m_tableCardsWidget->height());
            QFont hintFont = hint->font();
            hintFont.setPointSize(16);
            hint->setFont(hintFont);
            hint->setStyleSheet("QLabel { color: #4CAF50; }");
            hint->show();
        }
    }

    {
        QLayoutItem* child;
        while ((child = m_playerBLayout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        m_playerBLayout->addStretch();
        for (size_t i = 0; i < m_playerB.hand.size(); ++i)
            m_playerBLayout->insertWidget(
                static_cast<int>(m_playerBLayout->count() - 1),
                createCardBack());
    }

    if (m_dealAnimating) return;

    if (rebuildHand) {
        m_playerACardWidgets.clear();
        while (QLayoutItem* item = m_playerALayout->takeAt(0)) {
            if (QWidget* w = item->widget()) w->deleteLater();
            delete item;
        }

        for (const Card& card : m_playerA.hand) {
            CardWidget* cw = new CardWidget(card);
            connect(cw, &CardWidget::clicked, this, [this]() {
                update();
            });
            m_playerALayout->addWidget(cw);
            m_playerACardWidgets.push_back(cw);
        }
        m_playerALayout->addStretch();
    }

    if (!m_gameOver) {
        if (m_pendingPick) {
            m_buttonStack->setCurrentIndex(1);
            m_playButton->setEnabled(false);
        } else {
            m_buttonStack->setCurrentIndex(0);
            m_playButton->setEnabled(!m_waitingForAI && !m_playerA.hand.empty());
        }
        m_passButton->setEnabled(m_lastPlay.type != CardType::Invalid);
    }
}

void BattleScreen::endRound(Player& winner)
{
    settleScoreCards(winner, m_tableCards);
    winner.totalScore += m_tableBonus;

    int score = calculateTableScore(m_tableCards, m_tableBonus);
    if (score > 0) {
        appendLog(QString("%1 获得 %2 分%3")
            .arg(QString::fromStdString(winner.name))
            .arg(score)
            .arg(m_tableBonus > 0 ? QString(" (压分奖励 %1)").arg(m_tableBonus) : ""));
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
    appendLog(QString("--- 回合 %1 结束 ---").arg(m_roundCount));

    playSound(m_soundShine);

    if (m_tableFrame) {
        QString savedStyle = m_tableFrame->styleSheet();
        m_tableFrame->setStyleSheet(R"(
            QFrame {
                background-color: #0D3B16;
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

void BattleScreen::refillBoth(Player& winner, Player& loser)
{
    int beforeW = static_cast<int>(winner.hand.size());
    int beforeL = static_cast<int>(loser.hand.size());

    // 双牌堆：赢家和输家各自从自己的牌堆补牌
    Deck& winnerDeck = (winner.name == m_playerA.name) ? m_playerDeck : m_bossDeck;
    Deck& loserDeck  = (loser.name  == m_playerA.name) ? m_playerDeck : m_bossDeck;

    bool winnerIsPlayerA = (winner.name == m_playerA.name);
    bool loserIsPlayerA = (loser.name == m_playerA.name);

    refillToFive(winner, winnerDeck);
    refillToFive(loser, loserDeck);

    int gotW = static_cast<int>(winner.hand.size()) - beforeW;
    int gotL = static_cast<int>(loser.hand.size()) - beforeL;

    appendLog(QString("补牌: %1 +%2张 → %3张, %4 +%5张 → %6张")
        .arg(QString::fromStdString(winner.name)).arg(gotW).arg(static_cast<int>(winner.hand.size()))
        .arg(QString::fromStdString(loser.name)).arg(gotL).arg(static_cast<int>(loser.hand.size())));

    sortHandSmart(m_playerA.hand);

    // 播放摸牌动画
    if (gotW > 0) {
        QWidget* targetHand = winnerIsPlayerA ? m_playerAHandWidget : m_playerBHandWidget;
        if (targetHand) {
            QPoint handPos = targetHand->mapTo(this, QPoint(0, 0));
            for (int i = 0; i < gotW; ++i) {
                Card drawnCard = winner.hand[beforeW + i];
                QPoint targetPos(handPos.x() + (beforeW + i) * 90 + 20, handPos.y() + 10);
                QTimer::singleShot(i * 150, this,
                    [this, winnerIsPlayerA, drawnCard, targetPos]() {
                        playDrawAnimation(winnerIsPlayerA, drawnCard, targetPos);
                    });
            }
        }
    }
    if (gotL > 0) {
        QWidget* targetHand = loserIsPlayerA ? m_playerAHandWidget : m_playerBHandWidget;
        if (targetHand) {
            QPoint handPos = targetHand->mapTo(this, QPoint(0, 0));
            for (int i = 0; i < gotL; ++i) {
                Card drawnCard = loser.hand[beforeL + i];
                QPoint targetPos(handPos.x() + (beforeL + i) * 90 + 20, handPos.y() + 10);
                QTimer::singleShot((gotW * 150) + i * 150, this,
                    [this, loserIsPlayerA, drawnCard, targetPos]() {
                        playDrawAnimation(loserIsPlayerA, drawnCard, targetPos);
                    });
            }
        }
    }

    updateUI();
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
    m_gameOver = true;
    disableActionButtons();

    if (m_isLevelMode) {
        handleLevelModeEnd(m_playerA.totalScore >= m_playerB.totalScore);
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("游戏结束");
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
    cardsA->setStyleSheet("QLabel { color: #A5D6A7; font-size: 14px; }");
    cardsB->setStyleSheet("QLabel { color: #A5D6A7; font-size: 14px; }");
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
        m_gameOver = false;
        onNewGameButtonClicked();
    }
}