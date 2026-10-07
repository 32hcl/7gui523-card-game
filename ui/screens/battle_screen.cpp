#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/boss_avatar.h"
#include "ui/widgets/dialogue_overlay.h"
#include "ui/audio/audio_manager.h"
#include "ui/dialogs/card_picker.h"
#include "ui/router/screen_router.h"
#include "dialogue/dialogue_engine.h"
#include "ai/ai.h"
#include "ai/ai_levels.h"
#include "core/card/cardtype.h"
#include "core/card/rank.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>
#include <QDialog>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QFile>
#include <QCoreApplication>
#include <QPixmap>
#include <random>
#include <algorithm>
#include <map>

QString cardsToString(const std::vector<Card>& cards) {
    QString result;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) result += " ";
        QString cardStr = QString::fromStdString(cards[i].suit + cards[i].point);
        if (cards[i].score > 0) {
            cardStr += QString("(%1分)").arg(cards[i].score);
        }
        result += cardStr;
    }
    return result;
}

QString cardTypeToQString(CardType type) {
    return QString::fromStdString(cardTypeToString(type));
}

void sortHandSmart(std::vector<Card>& hand) {
    std::sort(hand.begin(), hand.end(),
        [](const Card& a, const Card& b) {
            int ra = getCardRank(a.point);
            int rb = getCardRank(b.point);
            if (ra != rb) return ra > rb;
            return a.suit < b.suit;
        });
}

void applyBtnShadow(QPushButton* btn)
{
    QGraphicsDropShadowEffect* s = new QGraphicsDropShadowEffect(btn);
    s->setBlurRadius(12);
    s->setOffset(0, 3);
    s->setColor(QColor(0, 0, 0, 120));
    btn->setGraphicsEffect(s);
}

BattleScreen::BattleScreen(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(R"(
        QWidget {
            color: #fff8e7;
            font-family: "Microsoft YaHei";
        }
        QLabel {
            color: #fff8e7;
        }
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e63946, stop:0.5 #c41e3a, stop:1 #9b1521);
            color: #fff8e7;
            border: 2px solid #ffd700;
            border-radius: 10px;
            padding: 6px 12px;
            font-size: 13px;
            font-weight: bold;
            min-width: 72px;
            min-height: 32px;
        }
        QPushButton:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f04a57, stop:0.5 #d6273f, stop:1 #a81824);
            border: 2px solid #ffe66d;
        }
        QPushButton:pressed {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #9b1521, stop:0.5 #7f1d1d, stop:1 #5c0e14);
            border: 2px solid #b8860b;
        }
        QPushButton:disabled {
            background-color: #3a2418;
            color: #8b7355;
            border: 2px solid #5a4030;
        }
        QTextEdit {
            background-color: #120a08;
            color: #c9b896;
            border: 1px solid rgba(255,215,0,0.25);
            border-radius: 8px;
            font-family: "Microsoft YaHei";
            font-size: 13px;
            padding: 8px;
        }
    )");

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(8, 4, 8, 4);
    rootLayout->setSpacing(10);

    auto* leftPanel = new QWidget;
    auto* leftLay   = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(4, 4, 4, 4);
    leftLay->setSpacing(10);

    createTopBar(leftLay);
    createOpponentArea(leftLay);
    createTableArea(leftLay);
    createPlayerArea(leftLay);

    rootLayout->addWidget(leftPanel, 1);

    QWidget* bottomBar = createBottomBar();
    createRightPanel(rootLayout, bottomBar);

    initFirstChoiceWidget();
    initGuessWidget();
    connectSignals();

    // 对话覆盖层（悬浮在最上层）
    m_dialogueOverlay = new DialogueOverlay(this);
    m_dialogueOverlay->raise();
    connect(m_dialogueOverlay, &DialogueOverlay::dialogueFinished,
            this, &BattleScreen::onDialogueFinished);

    startNewGame();
}

void BattleScreen::setLevelMode(bool isLevelMode, int startLevel)
{
    m_isLevelMode = isLevelMode;
    m_currentLevel = startLevel;
    m_levelRule = createLevelRule(startLevel);

    // 关卡模式隐藏难度选择按钮
    if (isLevelMode && m_difficultyButton) {
        m_difficultyButton->setVisible(false);
    }

    // 更新Boss头像和名称
    if (m_bossAvatar && m_bossAvatarWidget) {
        m_bossAvatar->setLevel(startLevel);
        if (auto* nameLabel = m_bossAvatarWidget->findChild<QLabel*>("bossNameLabel")) {
            nameLabel->setText(getLevelDisplayName(startLevel));
        }
    }
}

QString BattleScreen::getLevelDisplayName(int level) const
{
    return QString::fromLatin1(getBossName(level));
}

void BattleScreen::onReturnMenuClicked()
{
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    AUDIO_MGR.stopBgm(500);
    returnToMenu();
}

void BattleScreen::onDialogueFinished()
{
    DialogueCtx ctx = m_dialogueCtx;
    m_dialogueCtx = DialogueCtx::None;

    switch (ctx) {
        case DialogueCtx::PreBattle:
            // 战前对话结束，进入猜牌环节
            showGuessWidget();
            break;

        case DialogueCtx::PlayerSpecial:
            // 玩家出特殊牌后的对话结束，进入AI回合
            m_phase = GamePhase::AITurn;
            disableActionButtons();
            QTimer::singleShot(m_aiTurnDelayMs, this, &BattleScreen::doAITurn);
            break;

        case DialogueCtx::BossSpecial:
            // Boss出特殊牌后的对话结束，进入玩家回合
            if (m_phase != GamePhase::GameOver) {
                m_phase = GamePhase::PlayerTurn;
                enableActionButtons();
                updateUI();
            }
            break;

        case DialogueCtx::Victory:
        case DialogueCtx::Defeat:
            // 胜负对话结束，显示结算对话框
            showLevelEndDialog();
            break;

        case DialogueCtx::None:
            break;
    }
}

void BattleScreen::resetDialogueFlags()
{
    m_bossFirstBombShown = false;
    m_bossFirstRocketShown = false;
    m_playerFirstBombShown = false;
    m_playerFirstRocketShown = false;
    m_special523DialogShown = false;
}

void BattleScreen::disableActionButtons()
{
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);
    m_hpSwapButton->setEnabled(false);
}

void BattleScreen::enableActionButtons()
{
    if (m_phase == GamePhase::GameOver) return;
    m_playButton->setEnabled(m_phase == GamePhase::PlayerTurn && !m_playerA.hand.empty());
    m_passButton->setEnabled(m_phase == GamePhase::PlayerTurn && m_lastPlay.type != CardType::Invalid);
    bool canHpSwap = m_phase == GamePhase::PlayerTurn
                     && m_isLevelMode
                     && m_campaign.playerHp() > m_hpSwapCost
                     && !m_playerDeck.cards.empty();
    m_hpSwapButton->setEnabled(canHpSwap);
    m_shopButton->setVisible(m_isLevelMode);
    m_difficultyButton->setVisible(!m_isLevelMode);  // 关卡模式隐藏难度选择
}

void BattleScreen::appendLog(const QString& text)
{
    if (!m_logTextEdit) return;
    m_logTextEdit->append(text);
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}