#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/play_effect_overlay.h"
#include "ui/widgets/dialogue_overlay.h"
#include "ui/audio/audio_manager.h"
#include "ui/dialogs/card_picker.h"
#include "ui/dialogs/shop_dialog.h"
#include "dialogue/dialogue_engine.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QRandomGenerator>
#include <random>
#include <algorithm>
#include <set>

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

    CardTypeResult result = parseAnyCardType(selected);
    if (result.type == CardType::Invalid) {
        QMessageBox::warning(this, "非法牌型", "你选的牌不构成合法牌型");
        AUDIO_MGR.playSfx(AudioManager::Sfx::Lose);
        return;
    }

    bool canPass = canBeat(result, m_lastPlay);

    if (m_lastPlay.type != CardType::Invalid && !canPass) {
        QMessageBox::warning(this, "无法压过", "你选的牌无法压过上一手");
        AUDIO_MGR.playSfx(AudioManager::Sfx::Lose);
        return;
    }

    if (m_isLevelMode && m_levelRule.blockPlayer &&
        m_levelRule.blockPlayer(result, m_lastPlay)) {
        QMessageBox::warning(this, "无法压过", QString::fromUtf8(m_levelRule.blockPlayerMsg));
        AUDIO_MGR.playSfx(AudioManager::Sfx::Lose);
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

    std::set<int> removedHandIndices;
    for (const Card& c : selected) {
        auto it = m_playerA.hand.end();
        if (c.seq > 0) {
            it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
                [&](const Card& h) {
                    return h.seq == c.seq;
                });
        }
        if (it == m_playerA.hand.end()) {
            it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
                [&](const Card& h) {
                    return h.point == c.point && h.suit == c.suit;
                });
        }
        if (it != m_playerA.hand.end()) m_playerA.hand.erase(it);
    }
    m_handDirty = true;

    if (result.type == CardType::Bomb || result.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
        if (m_effectOverlay) {
            if (result.type == CardType::Rocket) {
                m_effectOverlay->showRocketEffect();
            } else {
                m_effectOverlay->showBombEffect();
            }
        }
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
        m_playerPlayedSpecial523 = true;
        appendLog("玩家A 达成七鬼523，直接获胜！");
        AUDIO_MGR.playSfx(AudioManager::Sfx::Special523);
        m_pendingSpecialVictory = true;
        if (m_effectOverlay) {
            m_effectOverlay->showSpecial523Effect();
        }
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

    if (result.type == CardType::Bomb) {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Bomb);
    } else if (result.type == CardType::Rocket) {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Rocket);
    } else if (m_lastPlay.bonusScore > 0) {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Win);
    } else {
        AUDIO_MGR.playSfx(AudioManager::Sfx::PlayCard);
    }

    updateUI();

    bool playerAFinished = m_playerA.hand.empty();

    if (playerAFinished && m_playerDeck.cards.empty()) {
        finishGame(m_playerA, "玩家A");
        return;
    }

    if (playerAFinished) {
        refillToFive(m_playerA, m_playerDeck);
        m_handDirty = true;
        updateUI(true, false);
        m_playButton->setEnabled(false);
    }

    // ── 关卡模式：特殊事件对话（炸弹/王炸第一次触发时） ──
    if (m_isLevelMode && m_dialogueOverlay && !m_dialogueOverlay->isPlaying()) {
        bool hasSpecial = false;
        DialogueTrigger trigger = DialogueTrigger::PreBattle;

        if (result.type == CardType::Bomb && !m_playerFirstBombShown) {
            trigger = DialogueTrigger::OnFirstBomb;
            m_playerFirstBombShown = true;
            hasSpecial = true;
        } else if (result.type == CardType::Rocket && !m_playerFirstRocketShown) {
            trigger = DialogueTrigger::OnFirstRocket;
            m_playerFirstRocketShown = true;
            hasSpecial = true;
        }

        if (hasSpecial) {
            auto lines = DialogueEngine::instance().getDialogue(m_currentLevel, trigger);
            if (!lines.isEmpty()) {
                m_dialogueCtx = DialogueCtx::PlayerSpecial;
                disableActionButtons();
                m_dialogueOverlay->playDialogue(lines, m_currentLevel);
                return;  // 对话结束后由 onDialogueFinished 进入AI回合
            }
        }
    }

    m_phase = GamePhase::AITurn;
    disableActionButtons();
    QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
}

void BattleScreen::autoPlayAllCards()
{
    if (m_phase != GamePhase::PlayerTurn) return;
    if (m_playerA.hand.empty()) return;

    std::vector<Card> chosen = dispatchAI(m_playerA, m_playerB, m_playerDeck);

    if (chosen.empty()) {
        appendLog("玩家A 自动出牌: 无合法牌型，出最小单张");
        chosen = {m_playerA.hand.front()};
    }

    std::set<int> usedWidgetIndices;
    std::vector<int> widgetSelectFlags(m_playerACardWidgets.size(), 0);

    for (const Card& c : chosen) {
        bool found = false;
        if (c.seq > 0) {
            for (size_t i = 0; i < m_playerACardWidgets.size(); ++i) {
                if (usedWidgetIndices.count(static_cast<int>(i))) continue;
                const Card& wc = m_playerACardWidgets[i]->getCard();
                if (wc.seq == c.seq) {
                    widgetSelectFlags[i] = 1;
                    usedWidgetIndices.insert(static_cast<int>(i));
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            for (size_t i = 0; i < m_playerACardWidgets.size(); ++i) {
                if (usedWidgetIndices.count(static_cast<int>(i))) continue;
                const Card& wc = m_playerACardWidgets[i]->getCard();
                if (wc.point == c.point && wc.suit == c.suit) {
                    widgetSelectFlags[i] = 1;
                    usedWidgetIndices.insert(static_cast<int>(i));
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            qWarning() << "[autoPlayAllCards] no widget match for card"
                       << QString::fromStdString(c.point) << QString::fromStdString(c.suit) << "seq=" << c.seq;
        }
    }

    for (size_t i = 0; i < m_playerACardWidgets.size(); ++i) {
        m_playerACardWidgets[i]->setSelected(widgetSelectFlags[i] == 1);
    }

    onPlayButtonClicked();
}

void BattleScreen::onPassButtonClicked()
{
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    if (m_phase != GamePhase::PlayerTurn) return;

    if (m_lastPlay.type == CardType::Invalid) {
        appendLog("首出不能不要");
        AUDIO_MGR.playSfx(AudioManager::Sfx::Lose);
        return;
    }

    appendLog("玩家A 不要");

    endRound(m_playerB);
    refillBoth(m_playerB, m_playerA);

    startNextTurn();
}

// 扣血换牌：消耗HP，弃掉选中的手牌（未选则弃最后一张）
// 弃牌放回牌堆后洗牌，再抽等量新牌，避免抽回刚弃的牌
void BattleScreen::onHpSwapButtonClicked()
{
    if (m_phase != GamePhase::PlayerTurn) return;
    if (!m_isLevelMode) return;
    if (m_campaign.playerHp() <= m_hpSwapCost) {
        appendLog("血量不足，无法扣血换牌！");
        return;
    }
    if (m_playerDeck.cards.empty()) {
        appendLog("牌堆已空，无法换牌！");
        return;
    }

    m_campaign.applyRoundDamage(0, m_hpSwapCost);
    appendLog(QString("扣血换牌: HP -%1").arg(m_hpSwapCost));

    std::vector<Card> discardCards;
    for (CardWidget* cw : m_playerACardWidgets) {
        if (cw->isSelected()) {
            discardCards.push_back(cw->getCard());
            cw->setSelected(false);
        }
    }

    if (discardCards.empty()) {
        if (m_playerA.hand.empty()) {
            appendLog("手牌为空，无法换牌！");
            return;
        }
        discardCards = {m_playerA.hand.back()};
    }

    for (const Card& c : discardCards) {
        auto it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
            [&](const Card& h) { return h.seq > 0 && h.seq == c.seq; });
        if (it == m_playerA.hand.end()) {
            it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
                [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
        }
        if (it != m_playerA.hand.end()) {
            Card discarded = *it;
            discarded.seq = ++m_playerNextSeq;
            m_playerDeck.cards.push_back(discarded);
            m_playerA.hand.erase(it);
        }
    }

    int swapCount = static_cast<int>(discardCards.size());
    thread_local std::mt19937 rng(std::random_device{}());
    std::shuffle(m_playerDeck.cards.begin(), m_playerDeck.cards.end(), rng);
    int actualDraw = std::min(swapCount, static_cast<int>(m_playerDeck.cards.size()));
    if (actualDraw > 0) {
        dealCards(m_playerA, m_playerDeck, actualDraw);
    }
    sortHandSmart(m_playerA.hand);
    m_handDirty = true;
    updateUI(true, false);
    enableActionButtons();
}

void BattleScreen::onNewGameButtonClicked()
{
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    for (CardWidget* cw : m_playerACardWidgets) {
        cw->deleteLater();
    }
    m_playerACardWidgets.clear();
    for (QWidget* cw : m_playerBCardWidgets) {
        cw->deleteLater();
    }
    m_playerBCardWidgets.clear();
    m_phase = GamePhase::DealAnimation;
    startNewGame();
}

void BattleScreen::onDifficultyButtonClicked()
{
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    switch (m_aiLevel) {
        case AILevel::AI1_Rule:
            m_aiLevel = AILevel::AI2_Greedy;
            m_difficultyButton->setText("难度: 贪心");
            break;
        case AILevel::AI2_Greedy:
            m_aiLevel = AILevel::AI3_Search;
            m_difficultyButton->setText("难度: 搜索");
            break;
        case AILevel::AI3_Search:
            m_aiLevel = AILevel::AI4_Search;
            m_difficultyButton->setText("难度: 深搜");
            break;
        case AILevel::AI4_Search:
            m_aiLevel = AILevel::AI1_Rule;
            m_difficultyButton->setText("难度: 规则");
            break;
    }
    appendLog(QString("电脑难度切换为: %1").arg(m_difficultyButton->text()));
}

void BattleScreen::onFirstBtnClicked()  { AUDIO_MGR.playSfx(AudioManager::Sfx::Click); startGameWithFirst(true); }
void BattleScreen::onSecondBtnClicked() { AUDIO_MGR.playSfx(AudioManager::Sfx::Click); startGameWithFirst(false); }
void BattleScreen::onRandomBtnClicked() {
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    std::random_device rd; std::mt19937 g(rd());
    std::uniform_int_distribution<> d(0, 1);
    startGameWithFirst(d(g) == 0);
}

// 关卡模式：显示猜牌界面，桌面显示牌背，左右按钮猜大鬼/小鬼
void BattleScreen::showGuessWidget()
{
    if (!m_guessWidget) return;
    m_guessWidget->setGeometry(m_tableCardsWidget->rect());
    m_guessWidget->show();
    m_guessWidget->raise();

    auto* midRow = m_guessWidget->findChild<QHBoxLayout*>();
    if (midRow) {
        for (int i = 0; i < midRow->count(); ++i) {
            QWidget* w = midRow->itemAt(i)->widget();
            if (w) w->show();
        }
    }

    if (m_guessCardLabel) {
        // 每次显示先重置为牌背，不保留上次翻牌结果
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            m_guessCardLabel->setPixmap(QPixmap(backPath).scaled(100, 140,
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        m_guessCardLabel->show();
    }

    // 重置标题文字和样式，不保留上次猜牌结果
    auto* title = m_guessWidget->findChild<QLabel*>();
    if (title) {
        title->setText("猜牌决定先手");
        title->setStyleSheet("color: #FFD700; background: transparent; font-size: 18px; font-weight: bold;");
    }
}

// 猜牌处理：随机决定大鬼/小鬼，600ms后翻牌揭晓，猜对玩家先手
void BattleScreen::onGuessJoker(bool guessedBig)
{
    if (!m_guessWidget) return;
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);

    auto* midRow = m_guessWidget->findChild<QHBoxLayout*>();
    if (midRow) {
        for (int i = 0; i < midRow->count(); ++i) {
            QWidget* w = midRow->itemAt(i)->widget();
            if (w) w->hide();
        }
    }

    auto* title = m_guessWidget->findChild<QLabel*>();
    if (title) title->setText("揭晓...");

    bool actualIsBig = (QRandomGenerator::global()->bounded(2) == 0);
    bool correct = (guessedBig == actualIsBig);

    QString cardPath = QCoreApplication::applicationDirPath()
        + "/cards/" + CardWidget::cardImageFileName(
            Card{actualIsBig ? "大鬼" : "小鬼", "", 0, ++m_playerNextSeq});

    QTimer::singleShot(600, this, [this, correct, cardPath]() {
        if (!m_guessCardLabel) return;
        if (QFile::exists(cardPath)) {
            m_guessCardLabel->setPixmap(QPixmap(cardPath).scaled(100, 140,
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_guessCardLabel->setText(QString("??"));
        }

        auto* title = m_guessWidget->findChild<QLabel*>();
        if (title) {
            title->setText(correct ? "✓ 猜对了！你先手" : "✗ 猜错了！电脑先手");
            title->setStyleSheet(QString("color: %1; background: transparent; font-size: 18px; font-weight: bold;")
                .arg(correct ? "#2ECC71" : "#E74C3C"));
        }
        AUDIO_MGR.playSfx(correct ? AudioManager::Sfx::Win : AudioManager::Sfx::Lose);

        QTimer::singleShot(1500, this, [this, correct]() {
            if (m_guessWidget) m_guessWidget->hide();
            m_playerAIsFirst = correct;
            continueLevelGameAfterGuess();
        });
    });
}

void BattleScreen::onPickButtonClicked()
{
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
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

// 游戏中随时打开商店，购买回血药水或特殊卡牌
// 对局中只能浏览，关卡结算时才能购买
void BattleScreen::onShopButtonClicked()
{
    if (!m_isLevelMode) {
        QMessageBox::information(this, "提示", "商店仅在关卡模式中可用");
        return;
    }
    if (m_phase == GamePhase::GameOver || m_phase == GamePhase::DealAnimation) {
        QMessageBox::information(this, "提示", "当前阶段无法打开商店");
        return;
    }

    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    showShopDialog(true);  // 对局中 viewOnly
}