#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/dialogs/card_picker.h"
#include "ui/router/screen_router.h"
#include "ai/ai.h"
#include "ai/ai_levels.h"
#include "core/card/cardtype.h"
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
            int ra = RANK_MAP.at(a.point);
            int rb = RANK_MAP.at(b.point);
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
            color: #FFFFFF;
            font-family: "Microsoft YaHei";
        }
        QLabel {
            color: #FFFFFF;
        }
        QPushButton {
            background-color: #E8503A;
            color: #FFFFFF;
            border: 2px solid #FFFFFF;
            border-radius: 10px;
            padding: 8px 20px;
            font-size: 14px;
            font-weight: bold;
            min-width: 90px;
            min-height: 32px;
        }
        QPushButton:hover {
            background-color: #F0634E;
        }
        QPushButton:pressed {
            background-color: #C43D28;
        }
        QPushButton:disabled {
            background-color: #555555;
            color: #999999;
            border-color: #777777;
        }
        QTextEdit {
            background-color: #1e1e2a;
            color: #ECEFF1;
            border: 1px solid #FFFFFF;
            border-radius: 8px;
            font-family: "Microsoft YaHei";
            font-size: 13px;
            padding: 8px;
        }
    )");

    initSounds();

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
    connectSignals();

    startNewGame();
}

void BattleScreen::setLevelMode(bool isLevelMode, int startLevel)
{
    m_isLevelMode = isLevelMode;
    m_currentLevel = startLevel;
}

QString BattleScreen::getLevelDisplayName(int level) const
{
    return QString::fromLatin1(getBossName(level));
}

void BattleScreen::onReturnMenuClicked()
{
    playSound(m_soundClick);
    returnToMenu();
}

void BattleScreen::disableActionButtons()
{
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);
}

void BattleScreen::enableActionButtons()
{
    if (m_phase == GamePhase::GameOver) return;
    m_playButton->setEnabled(m_phase == GamePhase::PlayerTurn && !m_playerA.hand.empty());
    m_passButton->setEnabled(m_phase == GamePhase::PlayerTurn && m_lastPlay.type != CardType::Invalid);
}

void BattleScreen::appendLog(const QString& text)
{
    if (!m_logTextEdit) return;
    m_logTextEdit->append(text);
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}