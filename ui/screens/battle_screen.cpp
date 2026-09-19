#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/dialogs/card_picker.h"
#include "ai/ai.h"
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

static QString cardsToString(const std::vector<Card>& cards) {
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

static QString cardTypeToQString(CardType type) {
    return QString::fromStdString(cardTypeToString(type));
}

static void sortHandSmart(std::vector<Card>& hand) {
    std::map<std::string, int> countMap;
    for (const Card& c : hand) countMap[c.point]++;

    auto getRank = [&](const Card& c) -> int {
        auto it = RANK_MAP.find(c.point);
        return (it != RANK_MAP.end()) ? it->second : 0;
    };

    auto getTypePriority = [&](const Card& c) -> int {
        int count = countMap[c.point];
        if (c.point == "大鬼" || c.point == "小鬼") {
            bool hasBig   = countMap.count("大鬼") > 0;
            bool hasSmall = countMap.count("小鬼") > 0;
            if (hasBig && hasSmall) return 100;
            return 50;
        }
        if (count == 4) return 90;
        if (count == 3) return 70;
        if (count == 2) return 50;
        return 30;
    };

    std::sort(hand.begin(), hand.end(),
        [&](const Card& a, const Card& b) {
            int pa = getTypePriority(a);
            int pb = getTypePriority(b);
            if (pa != pb) return pa > pb;
            int ra = getRank(a);
            int rb = getRank(b);
            if (ra != rb) return ra > rb;
            return a.suit < b.suit;
        });
}

static void applyBtnShadow(QPushButton* btn)
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
            background-color: #2E7D32;
            color: #FFFFFF;
            border: 2px solid #66BB6A;
            border-radius: 10px;
            padding: 8px 20px;
            font-size: 14px;
            font-weight: bold;
            min-width: 90px;
            min-height: 32px;
        }
        QPushButton:hover {
            background-color: #388E3C;
            border-color: #81C784;
        }
        QPushButton:pressed {
            background-color: #1B5E20;
        }
        QPushButton:disabled {
            background-color: #555555;
            color: #999999;
            border-color: #777777;
        }
        QTextEdit {
            background-color: #1A2428;
            color: #ECEFF1;
            border: 1px solid #37474F;
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

void BattleScreen::createTopBar(QVBoxLayout* leftLay)
{
    auto* topBar = new QWidget;
    topBar->setFixedHeight(50);
    topBar->setStyleSheet("QWidget { background-color: #0D3B16; border-radius: 8px; }");
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(20, 0, 20, 0);

    auto* titleLabel = new QLabel("7鬼523斗地主变体");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("QLabel { color: #FFD700; }");

    m_deckCountLabel = new QLabel("牌堆: 44");
    m_roundLabel = new QLabel("回合: 1");
    QFont infoFont = m_deckCountLabel->font();
    infoFont.setPointSize(14);
    infoFont.setBold(true);
    m_deckCountLabel->setFont(infoFont);
    m_roundLabel->setFont(infoFont);
    m_deckCountLabel->setStyleSheet("QLabel { color: #FFF59D; }");
    m_roundLabel->setStyleSheet("QLabel { color: #FFF59D; }");

    topLay->addWidget(titleLabel);
    topLay->addStretch();
    topLay->addWidget(m_deckCountLabel);
    topLay->addSpacing(30);
    topLay->addWidget(m_roundLabel);

    QGraphicsDropShadowEffect* topBarShadow = new QGraphicsDropShadowEffect(topBar);
    topBarShadow->setBlurRadius(14);
    topBarShadow->setOffset(0, 4);
    topBarShadow->setColor(QColor(0, 0, 0, 130));
    topBar->setGraphicsEffect(topBarShadow);

    leftLay->addWidget(topBar);
}

void BattleScreen::createOpponentArea(QVBoxLayout* leftLay)
{
    m_playerBHandWidget = new QWidget;
    m_playerBLayout     = new QHBoxLayout(m_playerBHandWidget);
    m_playerBLayout->setContentsMargins(0, 0, 0, 0);
    m_playerBLayout->setSpacing(6);
    m_playerBLayout->addStretch();

    leftLay->addWidget(m_playerBHandWidget);
    m_playerBHandWidget->setFixedHeight(100);
}

void BattleScreen::createTableArea(QVBoxLayout* leftLay)
{
    m_tableFrame = new QFrame;
    m_tableFrame->setFrameShape(QFrame::StyledPanel);
    m_tableFrame->setStyleSheet(R"(
        QFrame {
            background-color: #0D3B16;
            border: 1px solid #1B5E20;
            border-radius: 12px;
        }
    )");
    m_tableFrame->setMinimumHeight(280);

    QGraphicsDropShadowEffect* tableShadow = new QGraphicsDropShadowEffect(m_tableFrame);
    tableShadow->setBlurRadius(20);
    tableShadow->setOffset(0, 8);
    tableShadow->setColor(QColor(0, 0, 0, 160));
    m_tableFrame->setGraphicsEffect(tableShadow);

    auto* tableOuterLay = new QHBoxLayout(m_tableFrame);
    tableOuterLay->setContentsMargins(0, 0, 0, 0);

    auto* leftBox = new QWidget;
    auto* tableLay = new QVBoxLayout(leftBox);
    tableLay->setContentsMargins(20, 15, 10, 15);
    tableLay->setSpacing(10);

    m_handTypeLabel = new QLabel("上一手牌型: 无");
    QFont tf = m_handTypeLabel->font();
    tf.setPointSize(13);
    m_handTypeLabel->setFont(tf);
    m_handTypeLabel->setStyleSheet("QLabel { color: #B0BEC5; }");
    m_handTypeLabel->setAlignment(Qt::AlignCenter);
    tableLay->addWidget(m_handTypeLabel);

    m_tableCardsWidget = new QWidget;
    m_tableCardsWidget->setFixedHeight(180);
    m_tableCardsWidget->setAttribute(Qt::WA_StyledBackground, true);
    tableLay->addWidget(m_tableCardsWidget);

    m_tableScoreLabel = new QLabel("原始分: 0 分 | 奖励分: 0 分 | 合计: 0 分");
    QFont tableFont = m_tableScoreLabel->font();
    tableFont.setPointSize(14);
    tableFont.setBold(true);
    m_tableScoreLabel->setFont(tableFont);
    m_tableScoreLabel->setStyleSheet("QLabel { color: #FFEB3B; }");
    m_tableScoreLabel->setAlignment(Qt::AlignCenter);
    tableLay->addWidget(m_tableScoreLabel);

    tableOuterLay->addWidget(leftBox, 65);

    m_deckDisplayWidget = new QWidget;
    m_deckDisplayWidget->setStyleSheet("background-color: #0A2E12; border-left: 2px solid #558B2F;");
    m_deckDisplayLayout = new QVBoxLayout(m_deckDisplayWidget);
    m_deckDisplayLayout->setContentsMargins(10, 15, 10, 15);
    m_deckDisplayLayout->setAlignment(Qt::AlignCenter);

    auto* deckTitle = new QLabel("牌堆");
    deckTitle->setAlignment(Qt::AlignCenter);
    deckTitle->setStyleSheet("QLabel { color: #FFD700; font-size: 14px; font-weight: bold; }");
    m_deckDisplayLayout->addWidget(deckTitle);

    m_deckBackLabel = new QLabel;
    m_deckBackLabel->setFixedSize(150, 220);
    m_deckBackLabel->setAlignment(Qt::AlignCenter);
    m_deckBackLabel->setStyleSheet("QLabel { background: transparent; border: none; }");
    {
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            QPixmap backPix(backPath);
            QPixmap scaled = backPix.scaled(146, 214, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            QWidget* deckStack = new QWidget;
            deckStack->setFixedSize(156, 226);
            deckStack->setStyleSheet("background: transparent;");

            for (int layer = 5; layer >= 0; --layer) {
                QLabel* layerLabel = new QLabel(deckStack);
                layerLabel->setFixedSize(150, 220);
                layerLabel->setAlignment(Qt::AlignCenter);
                layerLabel->setStyleSheet("QLabel { background: transparent; border: none; }");
                layerLabel->setPixmap(scaled);
                layerLabel->move(-layer * 2, layer * 2);

                if (layer > 0) {
                    QGraphicsOpacityEffect* op = new QGraphicsOpacityEffect(layerLabel);
                    op->setOpacity(1.0 - layer * 0.12);
                    layerLabel->setGraphicsEffect(op);
                }
                if (layer == 0) {
                    m_deckBackLabel = layerLabel;
                }
            }

            m_deckDisplayLayout->addWidget(deckStack, 0, Qt::AlignCenter);
        } else {
            m_deckDisplayLayout->addWidget(m_deckBackLabel, 0, Qt::AlignCenter);
        }
    }

    m_deckCountBigLabel = new QLabel("44 张");
    m_deckCountBigLabel->setAlignment(Qt::AlignCenter);
    m_deckCountBigLabel->setStyleSheet(
        "QLabel { color: #FFEB3B; font-size: 16px; font-weight: bold; }");
    m_deckDisplayLayout->addWidget(m_deckCountBigLabel);

    m_deckDisplayLayout->addStretch();

    tableOuterLay->addWidget(m_deckDisplayWidget, 30);

    leftLay->addWidget(m_tableFrame);
}

void BattleScreen::createPlayerArea(QVBoxLayout* leftLay)
{
    m_playerAHandWidget = new QWidget;
    m_playerALayout     = new QHBoxLayout(m_playerAHandWidget);
    m_playerALayout->setContentsMargins(0, 0, 0, 0);
    m_playerALayout->setSpacing(6);

    leftLay->addWidget(m_playerAHandWidget);
    m_playerAHandWidget->setFixedHeight(180);
}

QWidget* BattleScreen::createBottomBar()
{
    QWidget* bottomBar = new QWidget;
    auto* bottomLay = new QVBoxLayout(bottomBar);
    bottomLay->setContentsMargins(0, 8, 0, 0);
    bottomLay->setSpacing(6);

    auto* scoreRow = new QHBoxLayout;
    m_scoreALabel = new QLabel("玩家A: 0 分");
    m_scoreBLabel = new QLabel("电脑: 0 分");
    m_scoreALabel->setStyleSheet("QLabel { color: #FFD700; font-size: 14px; font-weight: bold; }");
    m_scoreBLabel->setStyleSheet("QLabel { color: #FFD700; font-size: 14px; font-weight: bold; }");
    scoreRow->addWidget(m_scoreALabel);
    scoreRow->addStretch();
    scoreRow->addWidget(m_scoreBLabel);
    bottomLay->addLayout(scoreRow);

    auto* btnGrid = new QGridLayout;
    btnGrid->setSpacing(6);

    m_playButton    = new QPushButton("出牌");
    m_passButton    = new QPushButton("不要");
    m_pickButton    = new QPushButton("选卡");
    m_difficultyButton = new QPushButton("难度: 电脑1");
    m_newGameButton = new QPushButton("重新开始");

    m_playButton->setMinimumHeight(34);
    m_passButton->setMinimumHeight(34);
    m_pickButton->setMinimumHeight(34);
    m_difficultyButton->setMinimumHeight(34);
    m_newGameButton->setMinimumHeight(34);

    applyBtnShadow(m_playButton);
    applyBtnShadow(m_passButton);
    applyBtnShadow(m_pickButton);
    applyBtnShadow(m_difficultyButton);
    applyBtnShadow(m_newGameButton);

    m_buttonStack = new QStackedWidget;
    m_buttonStack->addWidget(m_passButton);
    m_buttonStack->addWidget(m_pickButton);
    m_buttonStack->setMinimumHeight(34);

    btnGrid->addWidget(m_playButton,       0, 0);
    btnGrid->addWidget(m_buttonStack,      0, 1);
    btnGrid->addWidget(m_difficultyButton, 1, 0, 1, 2);
    btnGrid->addWidget(m_newGameButton,    2, 0, 1, 2);

    bottomLay->addLayout(btnGrid);

    return bottomBar;
}

void BattleScreen::createRightPanel(QHBoxLayout* rootLayout, QWidget* bottomBar)
{
    auto* rightPanel = new QWidget;
    auto* rightLay   = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);

    auto* logLabel = new QLabel("游戏日志");
    QFont logFont = logLabel->font();
    logFont.setPointSize(15);
    logFont.setBold(true);
    logLabel->setFont(logFont);
    logLabel->setStyleSheet("QLabel { color: #FFD700; }");
    rightLay->addWidget(logLabel);

    m_logTextEdit = new QTextEdit;
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMinimumWidth(260);
    m_logTextEdit->setMaximumWidth(400);
    rightLay->addWidget(m_logTextEdit, 1);

    rightLay->addWidget(bottomBar);

    rootLayout->addWidget(rightPanel);
}

void BattleScreen::initFirstChoiceWidget()
{
    m_firstChoiceWidget = new QWidget(m_tableCardsWidget);
    m_firstChoiceWidget->setGeometry(0, 0, 400, 160);
    m_firstChoiceWidget->setStyleSheet("background: transparent;");
    auto* fcLayout = new QHBoxLayout(m_firstChoiceWidget);
    fcLayout->setAlignment(Qt::AlignCenter);
    fcLayout->setSpacing(20);

    auto* fb = new QPushButton("先手");
    auto* rb = new QPushButton("随机");
    auto* sb = new QPushButton("后手");
    {
        QFont f = fb->font();
        f.setPointSize(16); f.setBold(true);
        fb->setFont(f); rb->setFont(f); sb->setFont(f);
    }
    fb->setMinimumSize(120, 50);
    rb->setMinimumSize(120, 50);
    sb->setMinimumSize(120, 50);

    applyBtnShadow(fb);
    applyBtnShadow(rb);
    applyBtnShadow(sb);

    fb->setObjectName("firstBtn");
    rb->setObjectName("randomBtn");
    sb->setObjectName("secondBtn");

    fcLayout->addWidget(fb);
    fcLayout->addWidget(rb);
    fcLayout->addWidget(sb);

    m_firstChoiceWidget->hide();
}

void BattleScreen::connectSignals()
{
    auto* fb = m_firstChoiceWidget->findChild<QPushButton*>("firstBtn");
    auto* rb = m_firstChoiceWidget->findChild<QPushButton*>("randomBtn");
    auto* sb = m_firstChoiceWidget->findChild<QPushButton*>("secondBtn");
    if (fb) connect(fb, &QPushButton::clicked, this, &BattleScreen::onFirstBtnClicked);
    if (rb) connect(rb, &QPushButton::clicked, this, &BattleScreen::onRandomBtnClicked);
    if (sb) connect(sb, &QPushButton::clicked, this, &BattleScreen::onSecondBtnClicked);

    connect(m_playButton,       &QPushButton::clicked, this, &BattleScreen::onPlayButtonClicked);
    connect(m_passButton,       &QPushButton::clicked, this, &BattleScreen::onPassButtonClicked);
    connect(m_pickButton,       &QPushButton::clicked, this, &BattleScreen::onPickButtonClicked);
    connect(m_difficultyButton, &QPushButton::clicked, this, &BattleScreen::onDifficultyButtonClicked);
    connect(m_newGameButton,    &QPushButton::clicked, this, &BattleScreen::onNewGameButtonClicked);
}

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

    if (playerAFinished && m_deck.cards.empty()) {
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

    if (m_playerB.hand.empty() && !m_deck.cards.empty()) {
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

    if (m_playerB.hand.empty() && !m_deck.cards.empty()) {
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

    for (const Card& c : m_pickedCards) {
        auto it = std::find_if(m_deck.cards.begin(), m_deck.cards.end(),
            [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
        if (it != m_deck.cards.end()) m_deck.cards.erase(it);
    }

    shuffleDeck(m_deck);

    emit gameStarted();

    m_playerA.hand = m_pickedCards;
    int needA = 5 - (int)m_playerA.hand.size();
    if (needA > 0) dealCards(m_playerA, m_deck, needA);
    sortHandSmart(m_playerA.hand);

    dealCards(m_playerB, m_deck, 5);

    appendLog(QString("玩家A 手牌: %1").arg(cardsToString(m_playerA.hand)));
    appendLog(QString("电脑 手牌: %1").arg(cardsToString(m_playerB.hand)));
    appendLog(QString("牌堆剩余: %1 张").arg((int)m_deck.cards.size()));
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
    std::vector<Card> chosen = aiChoosePlay(
        m_playerB, m_playerA, m_lastPlay, m_deck, tableScore, m_tracker);

    if (chosen.empty()) {
        appendLog("电脑 不要");

        if (m_playerA.hand.empty() && !m_deck.cards.empty()) {
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

        if (!m_deck.cards.empty()) {
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

    if (playerBFinished && m_deck.cards.empty()) {
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

    if (m_playerA.hand.empty() && !m_deck.cards.empty()) {
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

    m_playerA = createPlayer("玩家A");
    m_playerB = createPlayer("电脑");
    m_playerA.isHuman = true;
    m_playerB.aiLevel = m_aiLevel;
    m_tracker.reset();
    m_roundCount = 1;

    m_logTextEdit->clear();
    appendLog("========== 新游戏开始 ==========");
    appendLog("请选择先手方（可先选卡）");

    if (m_firstChoiceWidget) {
        m_firstChoiceWidget->setVisible(true);
        m_firstChoiceWidget->raise();
    }

    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);
    m_buttonStack->setCurrentIndex(1);

    updateUI();
    emit gameStarted();
}

void BattleScreen::updateUI(bool rebuildHand)
{
    m_deckCountLabel->setText(
        QString("牌堆剩余: %1").arg(static_cast<int>(m_deck.cards.size())));
    if (m_deckCountBigLabel) {
        m_deckCountBigLabel->setText(
            QString("%1 张").arg(static_cast<int>(m_deck.cards.size())));
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
                .arg(QString::fromStdString(m_lastPlayerName)));
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
            for (QLabel* lbl : children) lbl->deleteLater();
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
    refillToFive(winner, m_deck);
    refillToFive(loser, m_deck);
    int gotW = static_cast<int>(winner.hand.size()) - beforeW;
    int gotL = static_cast<int>(loser.hand.size()) - beforeL;

    appendLog(QString("补牌: %1 +%2张 → %3张, %4 +%5张 → %6张")
        .arg(QString::fromStdString(winner.name)).arg(gotW).arg(static_cast<int>(winner.hand.size()))
        .arg(QString::fromStdString(loser.name)).arg(gotL).arg(static_cast<int>(loser.hand.size())));

    sortHandSmart(m_playerA.hand);
}

bool BattleScreen::checkGameEnd(Player& finisher, Player& opponent)
{
    if (!finisher.hand.empty()) return false;
    if (!m_deck.cards.empty()) return false;

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
    emit gameEnded();
    disableActionButtons();

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

void BattleScreen::disableActionButtons()
{
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);
}

void BattleScreen::enableActionButtons()
{
    if (m_gameOver) return;
    m_playButton->setEnabled(!m_waitingForAI && !m_playerA.hand.empty());
    m_passButton->setEnabled(!m_waitingForAI && m_lastPlay.type != CardType::Invalid);
}

void BattleScreen::appendLog(const QString& text)
{
    if (!m_logTextEdit) return;
    m_logTextEdit->append(text);
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

QWidget* BattleScreen::createCardBack()
{
    const int w = 65;
    const int h = 95;

    QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";

    QLabel* back = new QLabel;
    back->setFixedSize(w, h);
    back->setScaledContents(true);

    if (QFile::exists(backPath)) {
        QPixmap pix(backPath);
        back->setPixmap(pix.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        back->setStyleSheet("background-color: #888888; border: 2px solid #555555; border-radius: 8px;");
    }

    QGraphicsDropShadowEffect* backShadow = new QGraphicsDropShadowEffect(back);
    backShadow->setBlurRadius(10);
    backShadow->setOffset(0, 3);
    backShadow->setColor(QColor(0, 0, 0, 120));
    back->setGraphicsEffect(backShadow);

    return back;
}

void BattleScreen::layoutTableCards() {
    if (m_firstChoiceWidget) {
        m_firstChoiceWidget->setGeometry(0, 0,
            m_tableCardsWidget->width(), m_tableCardsWidget->height());
    }

    if (m_tableCardWidgets.empty()) return;

    int n = (int)m_tableCardWidgets.size();
    int cardW = 100;
    int spacing = 20;
    int totalWidth = n * cardW + (n - 1) * spacing;

    int startX = (m_tableCardsWidget->width() - totalWidth) / 2;
    int y = (m_tableCardsWidget->height() - 150) / 2;

    for (int i = 0; i < n; ++i) {
        CardWidget* cw = m_tableCardWidgets[i];
        cw->setParent(m_tableCardsWidget);
        cw->move(startX + i * (cardW + spacing), y);
        cw->show();
        cw->raise();
    }
}

void BattleScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_firstChoiceWidget && m_tableCardsWidget) {
        m_firstChoiceWidget->setGeometry(m_tableCardsWidget->rect());
    }
    layoutTableCards();
}