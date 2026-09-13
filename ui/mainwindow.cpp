#include "mainwindow.h"
#include "cardwidget.h"
#include "cardpickerdialog.h"
#include "../core/ai.h"
#include "../core/cardtype.h"
#include "../core/score.h"
#include "../core/special.h"
#include "../core/game.h"

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

// ── 卡牌图片文件名（与 CardWidget::cardImageFileName 一致） ──

static QString cardImageFileName(const Card& card) {
    if (card.point == "大鬼") return "card_joker_red.png";
    if (card.point == "小鬼") return "card_joker_black.png";

    QString suitKey;
    if (card.suit == "黑桃") suitKey = "spades";
    else if (card.suit == "红桃") suitKey = "hearts";
    else if (card.suit == "梅花") suitKey = "clubs";
    else if (card.suit == "方块") suitKey = "diamonds";
    else return QString();

    QString pointKey = QString::fromStdString(card.point);
    if (pointKey != "A" && pointKey != "J" && pointKey != "Q" && pointKey != "K") {
        bool ok = false;
        int num = pointKey.toInt(&ok);
        if (ok) pointKey = QString("%1").arg(num, 2, 10, QChar('0'));
    }
    return QString("card_%1_%2.png").arg(suitKey).arg(pointKey);
}

// ── 手牌智能排序 ──────────────────────────────────────────────────

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
            if (hasBig && hasSmall) return 100;  // 王炸
            return 50;
        }
        if (count == 4) return 90;   // 炸弹
        if (count == 3) return 70;   // 三张
        if (count == 2) return 50;   // 对子
        return 30;                    // 单张
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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("7鬼523斗地主变体");
    setMinimumSize(1200, 800);
    resize(1200, 800);

    setStyleSheet(R"(
        QMainWindow {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1B5E20, stop:0.5 #2E7D32, stop:1 #1B5E20);
        }
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

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(8, 4, 8, 4);
    rootLayout->setSpacing(10);

    auto* leftPanel = new QWidget;
    auto* leftLay   = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(4, 4, 4, 4);
    leftLay->setSpacing(10);

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

        leftLay->addWidget(topBar);
    }

    {
        m_playerBHandWidget = new QWidget;
        m_playerBLayout     = new QHBoxLayout(m_playerBHandWidget);
        m_playerBLayout->setContentsMargins(0, 0, 0, 0);
        m_playerBLayout->setSpacing(6);
        m_playerBLayout->addStretch();

        leftLay->addWidget(m_playerBHandWidget);
        m_playerBHandWidget->setFixedHeight(100);
    }

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

        auto* tableOuterLay = new QHBoxLayout(m_tableFrame);
        tableOuterLay->setContentsMargins(0, 0, 0, 0);

        // ── 左侧：原有内容（70%） ──
        auto* leftBox = new QWidget;
        auto* tableLay = new QVBoxLayout(leftBox);
        tableLay->setContentsMargins(20, 15, 10, 15);
        tableLay->setSpacing(10);

        // 顶部：上一手牌型
        m_handTypeLabel = new QLabel("上一手牌型: 无");
        QFont tf = m_handTypeLabel->font();
        tf.setPointSize(13);
        m_handTypeLabel->setFont(tf);
        m_handTypeLabel->setStyleSheet("QLabel { color: #B0BEC5; }");
        m_handTypeLabel->setAlignment(Qt::AlignCenter);
        tableLay->addWidget(m_handTypeLabel);

        // 中间：桌面牌显示区
        m_tableCardsWidget = new QWidget;
        m_tableCardsWidget->setFixedHeight(180);
        m_tableCardsWidget->setAttribute(Qt::WA_StyledBackground, true);
        tableLay->addWidget(m_tableCardsWidget);

        // 底部：桌面分
        m_tableScoreLabel = new QLabel("原始分: 0 分 | 奖励分: 0 分 | 合计: 0 分");
        QFont tableFont = m_tableScoreLabel->font();
        tableFont.setPointSize(14);
        tableFont.setBold(true);
        m_tableScoreLabel->setFont(tableFont);
        m_tableScoreLabel->setStyleSheet("QLabel { color: #FFEB3B; }");
        m_tableScoreLabel->setAlignment(Qt::AlignCenter);
        tableLay->addWidget(m_tableScoreLabel);

        tableOuterLay->addWidget(leftBox, 65);

        // ── 右侧：牌堆显示区（35%） ──
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
                m_deckBackLabel->setPixmap(
                    backPix.scaled(146, 214, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        m_deckDisplayLayout->addWidget(m_deckBackLabel, 0, Qt::AlignCenter);

        m_deckCountBigLabel = new QLabel("44 张");
        m_deckCountBigLabel->setAlignment(Qt::AlignCenter);
        m_deckCountBigLabel->setStyleSheet(
            "QLabel { color: #FFEB3B; font-size: 16px; font-weight: bold; }");
        m_deckDisplayLayout->addWidget(m_deckCountBigLabel);

        m_deckDisplayLayout->addStretch();

        tableOuterLay->addWidget(m_deckDisplayWidget, 30);

        leftLay->addWidget(m_tableFrame);
    }

    {
        m_playerAHandWidget = new QWidget;
        m_playerALayout     = new QHBoxLayout(m_playerAHandWidget);
        m_playerALayout->setContentsMargins(0, 0, 0, 0);
        m_playerALayout->setSpacing(6);

        leftLay->addWidget(m_playerAHandWidget);
        m_playerAHandWidget->setFixedHeight(180);
    }

    QWidget* bottomBar = nullptr;

    {
        bottomBar = new QWidget;
        auto* bottomLay = new QVBoxLayout(bottomBar);
        bottomLay->setContentsMargins(0, 8, 0, 0);
        bottomLay->setSpacing(6);

        // 第一行：分数 A + 分数 B
        auto* scoreRow = new QHBoxLayout;
        m_scoreALabel = new QLabel("玩家A: 0 分");
        m_scoreBLabel = new QLabel("玩家B: 0 分");
        m_scoreALabel->setStyleSheet("QLabel { color: #FFD700; font-size: 14px; font-weight: bold; }");
        m_scoreBLabel->setStyleSheet("QLabel { color: #FFD700; font-size: 14px; font-weight: bold; }");
        scoreRow->addWidget(m_scoreALabel);
        scoreRow->addStretch();
        scoreRow->addWidget(m_scoreBLabel);
        bottomLay->addLayout(scoreRow);

        // 第二行：按钮网格（两列）
        auto* btnGrid = new QGridLayout;
        btnGrid->setSpacing(6);

        m_playButton    = new QPushButton("出牌");
        m_passButton    = new QPushButton("不要");
        m_pickButton    = new QPushButton("选卡");
        m_difficultyButton = new QPushButton("难度: AI1");
        m_newGameButton = new QPushButton("重新开始");
        m_firstBtn      = new QPushButton("先手");
        m_randomBtn     = new QPushButton("随机");
        m_secondBtn     = new QPushButton("后手");

        m_playButton->setMinimumHeight(34);
        m_passButton->setMinimumHeight(34);
        m_pickButton->setMinimumHeight(34);
        m_difficultyButton->setMinimumHeight(34);
        m_newGameButton->setMinimumHeight(34);

        m_buttonStack = new QStackedWidget;
        m_buttonStack->addWidget(m_passButton);  // index 0
        m_buttonStack->addWidget(m_pickButton);  // index 1
        m_buttonStack->setMinimumHeight(34);

        connect(m_playButton,       &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
        connect(m_passButton,       &QPushButton::clicked, this, &MainWindow::onPassButtonClicked);
        connect(m_pickButton,       &QPushButton::clicked, this, &MainWindow::onPickButtonClicked);
        connect(m_difficultyButton, &QPushButton::clicked, this, &MainWindow::onDifficultyButtonClicked);
        connect(m_newGameButton,    &QPushButton::clicked, this, &MainWindow::onNewGameButtonClicked);
        connect(m_firstBtn,         &QPushButton::clicked, this, &MainWindow::onFirstBtnClicked);
        connect(m_randomBtn,        &QPushButton::clicked, this, &MainWindow::onRandomBtnClicked);
        connect(m_secondBtn,        &QPushButton::clicked, this, &MainWindow::onSecondBtnClicked);

        btnGrid->addWidget(m_playButton,       0, 0);
        btnGrid->addWidget(m_buttonStack,      0, 1);
        btnGrid->addWidget(m_firstBtn,         1, 0);
        btnGrid->addWidget(m_randomBtn,        1, 1);
        btnGrid->addWidget(m_secondBtn,        2, 0, 1, 2);
        btnGrid->addWidget(m_difficultyButton, 3, 0, 1, 2);
        btnGrid->addWidget(m_newGameButton,    4, 0, 1, 2);

        bottomLay->addLayout(btnGrid);

        // 不在 leftPanel 中添加 bottomBar，后面会加入 rightPanel
    }

    rootLayout->addWidget(leftPanel, 1);

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

    startNewGame();
}


void MainWindow::onPlayButtonClicked()
{
    if (m_waitingForAI || m_gameOver) return;

    if (m_isPicking) {
        m_isPicking = false;
        m_buttonStack->setCurrentIndex(0);
        appendLog("使用随机发牌");
    }

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

    // ── 飞行动画：手牌中的 CardWidget 飞到桌面 ──
    std::vector<CardWidget*> selectedWidgets;
    for (CardWidget* cw : m_playerACardWidgets) {
        if (cw->isSelected()) {
            selectedWidgets.push_back(cw);
        }
    }

    // 从手牌布局和列表移除
    for (CardWidget* cw : selectedWidgets) {
        m_playerALayout->removeWidget(cw);
        m_playerALayout->update();
        auto it = std::find(m_playerACardWidgets.begin(),
                            m_playerACardWidgets.end(), cw);
        if (it != m_playerACardWidgets.end()) {
            m_playerACardWidgets.erase(it);
        }
    }

    // 清空当前桌面牌（上一手残留）
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();

    // 飞行动画
    flyCardsToTable(selectedWidgets);

    // 从手牌数据移除
    for (const Card& c : selected) {
        auto it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
            [&](const Card& h) {
                return h.point == c.point && h.suit == c.suit;
            });
        if (it != m_playerA.hand.end()) m_playerA.hand.erase(it);
    }

    // 炸弹/王炸 — 桌面抖动
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

    // 检查刚打出的牌型是否为 Special523（非手牌检查）
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
        appendLog(QString("玩家B 总分: %1").arg(m_playerB.totalScore));
        QString w = (m_playerA.totalScore >= m_playerB.totalScore) ? "玩家A" : "玩家B";
        appendLog(QString("最终胜者: %1").arg(w));

        playSound(m_soundSuccess);
        showGameOverDialog(QString("玩家A 出完牌！\n玩家A: %1 分\n玩家B: %2 分")
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
    QTimer::singleShot(700, this, &MainWindow::doAITurn);
    if (playerAFinished) {
        m_playButton->setEnabled(false);
    }
}

void MainWindow::onPassButtonClicked()
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
        appendLog("玩家B 赢得本回合");
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
            appendLog("玩家B 达成七鬼523，直接获胜！");
            playSound(m_soundFailure);
            showSpecialVictoryEffect("玩家B", "玩家B 达成七鬼523，直接获胜！");
            return;
        }

        m_waitingForAI = true;
        QTimer::singleShot(700, this, &MainWindow::doAITurn);
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
        appendLog("玩家B 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showSpecialVictoryEffect("玩家B", "玩家B 达成七鬼523，直接获胜！");
        return;
    }

    m_lastPlay.type = CardType::Invalid;
    m_lastPlay.cards.clear();
    m_lastPlay.keyPoint.clear();
    m_lastPlayerName.clear();
    m_waitingForAI = true;
    QTimer::singleShot(700, this, &MainWindow::doAITurn);
}

void MainWindow::onNewGameButtonClicked()
{
    playSound(m_soundClick);
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    m_gameOver = false;
    startNewGame();
}

void MainWindow::onDifficultyButtonClicked()
{
    playSound(m_soundClick);
    switch (m_aiLevel) {
        case AILevel::AI1_Simple:
            m_aiLevel = AILevel::AI2_Rule;
            m_difficultyButton->setText("难度: AI2 规则");
            break;
        case AILevel::AI2_Rule:
            m_aiLevel = AILevel::AI3_Tracker;
            m_difficultyButton->setText("难度: AI3 记牌");
            break;
        case AILevel::AI3_Tracker:
            m_aiLevel = AILevel::AI4_Expert;
            m_difficultyButton->setText("难度: AI4 专家");
            break;
        case AILevel::AI4_Expert:
            m_aiLevel = AILevel::AI1_Simple;
            m_difficultyButton->setText("难度: AI1 简单");
            break;
    }
    appendLog(QString("AI 难度切换为: %1").arg(m_difficultyButton->text()));
}

void MainWindow::onFirstBtnClicked()
{
    playSound(m_soundClick);
    startGameWithFirst(true);
}

void MainWindow::onRandomBtnClicked()
{
    playSound(m_soundClick);
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<> dist(0, 1);
    startGameWithFirst(dist(g) == 0);
}

void MainWindow::onSecondBtnClicked()
{
    playSound(m_soundClick);
    startGameWithFirst(false);
}

void MainWindow::startGameWithFirst(bool playerAFirst)
{
    m_playerAIsFirst = playerAFirst;
    m_waitingForFirstChoice = false;

    m_firstBtn->setVisible(false);
    m_randomBtn->setVisible(false);
    m_secondBtn->setVisible(false);

    m_difficultyButton->setEnabled(true);
    m_newGameButton->setEnabled(true);

    appendLog(QString("先手: %1").arg(playerAFirst ? "玩家A" : "玩家B"));

    if (playerAFirst) {
        enableActionButtons();
        updateUI();
    } else {
        m_waitingForAI = true;
        disableActionButtons();
        updateUI();
        QTimer::singleShot(700, this, &MainWindow::doAITurn);
    }
}

void MainWindow::onPickButtonClicked()
{
    playSound(m_soundClick);
    CardPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        std::vector<Card> selected = dlg.selectedCards();

        Deck fullDeck = createStandardDeck();
        for (const Card& c : selected) {
            auto it = std::find_if(fullDeck.cards.begin(), fullDeck.cards.end(),
                [&](const Card& h) {
                    return h.point == c.point && h.suit == c.suit;
                });
            if (it != fullDeck.cards.end()) fullDeck.cards.erase(it);
        }

        shuffleDeck(fullDeck);

        m_playerA.hand = selected;
        int need = 5 - (int)selected.size();
        if (need > 0) {
            dealCards(m_playerA, fullDeck, need);
        }
        sortHandSmart(m_playerA.hand);
        m_deck = fullDeck;

        m_playerB.hand.clear();
        dealCards(m_playerB, m_deck, 5);

        appendLog("========== 自选起始手牌 ==========");
        appendLog(QString("玩家A 手牌: %1").arg(cardsToString(m_playerA.hand)));
        appendLog(QString("玩家B 手牌: %1").arg(cardsToString(m_playerB.hand)));
        appendLog(QString("牌堆剩余: %1 张").arg(static_cast<int>(m_deck.cards.size())));
    } else {
        appendLog("使用随机发牌");
    }

    m_isPicking = false;
    updateUI();
    enableActionButtons();
}

void MainWindow::doAITurn()
{
    m_waitingForAI = false;
    if (m_gameOver) return;

    int tableScore = calculateScore(m_tableCards);
    std::vector<Card> chosen = aiChoosePlay(
        m_playerB, m_playerA, m_lastPlay, m_deck, tableScore, m_tracker);

    if (chosen.empty()) {
        appendLog("玩家B 不要");

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
            appendLog("玩家B 达成七鬼523，直接获胜！");
            playSound(m_soundFailure);
            showSpecialVictoryEffect("玩家B", "玩家B 达成七鬼523，直接获胜！");
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
    m_lastPlayerName = "玩家B";

    // 炸弹/王炸 — 桌面抖动
    if (m_lastPlay.type == CardType::Bomb || m_lastPlay.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
    }

    m_tracker.recordPlayed(chosen);

    // AI 出牌：清空桌面 + 飞行动画
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();
    flyAICardsToTable(chosen);

    // 检查刚打出的牌型是否为 Special523
    if (m_lastPlay.type == CardType::Special523) {
        appendLog("玩家B 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showSpecialVictoryEffect("玩家B", "玩家B 达成七鬼523，直接获胜！");
        return;
    }

    int bonus = calculatePressureBonus(m_lastPlay, oldLastPlay);
    if (bonus > 0) {
        m_lastPlay.bonusScore = bonus;
        m_tableBonus += bonus;
    }
    if (bonus > 0) showBonusFloat(bonus);
    appendLog(QString("玩家B 出牌: %1 (%2%3)")
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
        appendLog("出完牌者: 玩家B");
        appendLog(QString("玩家A 总分: %1").arg(m_playerA.totalScore));
        appendLog(QString("玩家B 总分: %1").arg(m_playerB.totalScore));
        QString w = (m_playerA.totalScore >= m_playerB.totalScore) ? "玩家A" : "玩家B";
        appendLog(QString("最终胜者: %1").arg(w));

        playSound(m_soundFailure);
        showGameOverDialog(QString("玩家B 出完牌！\n玩家A: %1 分\n玩家B: %2 分")
            .arg(m_playerA.totalScore).arg(m_playerB.totalScore));
        disableActionButtons();
        return;
    }

    if (m_playerA.hand.empty() && !m_deck.cards.empty()) {
        appendLog("玩家B 出牌回应，玩家B 赢得本回合");
        endRound(m_playerB);
        refillBoth(m_playerB, m_playerA);
        m_lastPlay.type = CardType::Invalid;
        m_lastPlay.cards.clear();
        m_lastPlay.keyPoint.clear();
        m_lastPlayerName.clear();
        updateUI();
        m_waitingForAI = true;
        QTimer::singleShot(700, this, &MainWindow::doAITurn);
        return;
    }

    enableActionButtons();
    updateUI();
}

void MainWindow::startNewGame()
{
    for (CardWidget* cw : m_tableCardWidgets) {
        cw->deleteLater();
    }
    m_tableCardWidgets.clear();

    m_gameOver = false;
    m_waitingForAI = false;
    m_isPicking = false;
    m_lastPlay.type = CardType::Invalid;
    m_lastPlay.cards.clear();
    m_lastPlay.keyPoint.clear();
    m_tableCards.clear();
    m_lastPlayerName.clear();
    m_playerACardWidgets.clear();

    m_deck = createStandardDeck();
    shuffleDeck(m_deck);

    m_playerA = createPlayer("玩家A");
    m_playerB = createPlayer("玩家B");
    m_playerA.isHuman = true;
    m_playerB.aiLevel = m_aiLevel;

    m_tracker.reset();

    dealCards(m_playerA, m_deck, 5);
    dealCards(m_playerB, m_deck, 5);
    sortHandSmart(m_playerA.hand);
    m_roundCount = 1;

    m_logTextEdit->clear();
    appendLog("========== 新游戏开始 ==========");
    appendLog(QString("玩家A 初始手牌: %1").arg(cardsToString(m_playerA.hand)));
    appendLog(QString("玩家B 初始手牌: %1").arg(cardsToString(m_playerB.hand)));
    appendLog("请选择先手方");
    appendLog(QString("牌堆剩余: %1 张").arg(static_cast<int>(m_deck.cards.size())));

    if (static_cast<int>(m_deck.cards.size()) == 44) {
        m_isPicking = true;
    }

    m_waitingForFirstChoice = true;
    m_playerAIsFirst = true;
    m_firstBtn->setVisible(true);
    m_randomBtn->setVisible(true);
    m_secondBtn->setVisible(true);
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);
    m_difficultyButton->setEnabled(false);
    m_newGameButton->setEnabled(false);

    updateUI();
}

void MainWindow::updateUI()
{
    if (m_waitingForFirstChoice) {
        m_playButton->setEnabled(false);
        m_passButton->setEnabled(false);
    }

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
        QString("玩家B: %1 分").arg(m_playerB.totalScore));

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
    if (m_tableCardWidgets.empty()) {
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

    {
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
        if (m_isPicking) {
            m_buttonStack->setCurrentIndex(1);
            m_playButton->setEnabled(!m_waitingForAI && !m_playerA.hand.empty());
        } else {
            m_buttonStack->setCurrentIndex(0);
            m_playButton->setEnabled(!m_waitingForAI && !m_playerA.hand.empty());
        }
        m_passButton->setEnabled(m_lastPlay.type != CardType::Invalid);
    }
}

void MainWindow::endRound(Player& winner)
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

    // 桌面边框闪光
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

void MainWindow::refillBoth(Player& winner, Player& loser)
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

bool MainWindow::checkGameEnd(Player& finisher, Player& opponent)
{
    if (!finisher.hand.empty()) return false;
    if (!m_deck.cards.empty()) return false;

    endRound(finisher);
    finalSettlement(finisher, opponent, m_tableCards);
    compareAndAnnounce(m_playerA, m_playerB);

    appendLog("========== 游戏结束 ==========");
    appendLog(QString("出完牌者: %1").arg(QString::fromStdString(finisher.name)));
    appendLog(QString("玩家A 总分: %1").arg(m_playerA.totalScore));
    appendLog(QString("玩家B 总分: %1").arg(m_playerB.totalScore));

    QString w = (m_playerA.totalScore >= m_playerB.totalScore)
        ? "玩家A" : "玩家B";
    appendLog(QString("最终胜者: %1").arg(w));

    showGameOverDialog(QString("%1 出完牌！\n玩家A: %2 分\n玩家B: %3 分")
        .arg(QString::fromStdString(finisher.name))
        .arg(m_playerA.totalScore)
        .arg(m_playerB.totalScore));
    return true;
}

void MainWindow::showGameOverDialog(const QString& message)
{
    m_gameOver = true;
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
    auto* labelB = new QLabel(QString("玩家B: %1 分").arg(scoreB));
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
        .arg(aWin ? "玩家A" : "玩家B").arg(diff);
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
    auto* cardsB = new QLabel(QString("玩家B 分值牌: %1").arg(countScoreCards(m_playerB.collected)));
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

void MainWindow::disableActionButtons()
{
    m_playButton->setEnabled(false);
    m_passButton->setEnabled(false);
}

void MainWindow::enableActionButtons()
{
    if (m_gameOver) return;
    m_playButton->setEnabled(!m_waitingForAI && !m_playerA.hand.empty());
    m_passButton->setEnabled(!m_waitingForAI && m_lastPlay.type != CardType::Invalid);
}

void MainWindow::appendLog(const QString& text)
{
    if (!m_logTextEdit) return;
    m_logTextEdit->append(text);
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

QWidget* MainWindow::createCardBack()
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
    return back;
}

// ── 音效 ──────────────────────────────────────────────────────────

void MainWindow::initSounds()
{
    auto createPlayer = [](const QString& path,
                           QAudioOutput*& out,
                           QMediaPlayer*& player) {
        player = new QMediaPlayer();
        out = new QAudioOutput();
        player->setAudioOutput(out);
        out->setVolume(0.6);
        if (QFile::exists(path)) {
            player->setSource(QUrl::fromLocalFile(path));
        }
    };

    QString base = QCoreApplication::applicationDirPath() + "/music/";
    createPlayer(base + "成功.mp3", m_audioSuccess, m_soundSuccess);
    createPlayer(base + "失败.mp3", m_audioFailure, m_soundFailure);
    createPlayer(base + "正确.mp3", m_audioCorrect, m_soundCorrect);
    createPlayer(base + "错误.mp3", m_audioWrong,   m_soundWrong);
    createPlayer(base + "点击.mp3", m_audioClick,   m_soundClick);
    createPlayer(base + "赌场-弹珠机-老虎机.mp3", m_audioCasino, m_soundCasino);
    createPlayer(base + "闪亮.mp3", m_audioShine,   m_soundShine);
}

void MainWindow::playSound(QMediaPlayer* player)
{
    if (!player) return;
    if (player->source().isEmpty()) return;
    player->stop();
    player->setPosition(0);
    player->play();
}

// ── 出牌飞行动画：CardWidget 从手牌飞到桌面 ──

void MainWindow::flyCardsToTable(const std::vector<CardWidget*>& cards) {
    if (cards.empty()) return;

    int n = (int)cards.size();
    int cardW = 100;
    int spacing = 20;
    int totalWidth = n * cardW + (n - 1) * spacing;
    int tableW = m_tableCardsWidget->width();
    int tableH = m_tableCardsWidget->height();

    QPoint tableOrigin = m_tableCardsWidget->mapTo(this, QPoint(0, 0));
    int startX = tableOrigin.x() + (tableW - totalWidth) / 2;
    int targetY = tableOrigin.y() + (tableH - 150) / 2;

    for (int i = 0; i < n; ++i) {
        CardWidget* cw = cards[i];

        QPoint startPos = cw->mapTo(this, QPoint(0, 0));

        cw->setParent(this);
        cw->move(startPos);
        cw->show();
        cw->raise();
        cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        cw->setFlying(true);

        QPoint endPos(startX + i * (cardW + spacing), targetY);

        QPropertyAnimation* anim = new QPropertyAnimation(cw, "pos");
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(startPos);
        anim->setEndValue(endPos);

        connect(anim, &QPropertyAnimation::finished, this,
            [this, cw, endPos]() {
                cw->setParent(m_tableCardsWidget);
                QPoint relPos = endPos - m_tableCardsWidget->mapTo(this, QPoint(0, 0));
                cw->move(relPos);
                cw->show();
                cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                m_tableCardWidgets.push_back(cw);
            });

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

// ── AI 出牌飞行动画 ──────────────────────────────────────────

void MainWindow::flyAICardsToTable(const std::vector<Card>& cards) {
    if (cards.empty()) return;
    if (!m_playerBHandWidget || !m_tableCardsWidget) return;

    QWidget* playArea = m_tableCardsWidget->parentWidget();
    QPoint playPos = playArea->mapTo(this, QPoint(0, 0));
    QPoint aiPos = m_playerBHandWidget->mapTo(this, QPoint(0, 0));
    QPoint startPos(playPos.x() + playArea->width() / 2,
                    aiPos.y() + m_playerBHandWidget->height() / 2);

    QPoint tablePos = m_tableCardsWidget->mapTo(this, QPoint(0, 0));
    int tableW = m_tableCardsWidget->width();
    int tableH = m_tableCardsWidget->height();

    int n = (int)cards.size();
    int cardW = 100;
    int spacing = 20;
    int totalWidth = n * cardW + (n - 1) * spacing;
    int startX = tablePos.x() + (tableW - totalWidth) / 2;
    int targetY = tablePos.y() + (tableH - 150) / 2;

    for (int i = 0; i < n; ++i) {
        CardWidget* cw = new CardWidget(cards[i], this);
        cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        cw->move(startPos);
        cw->show();
        cw->raise();
        cw->setFlying(true);

        QPoint endPos(startX + i * (cardW + spacing), targetY);

        QPropertyAnimation* anim = new QPropertyAnimation(cw, "pos");
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(startPos);
        anim->setEndValue(endPos);

        connect(anim, &QPropertyAnimation::finished, this,
            [this, cw, endPos]() {
                cw->setParent(m_tableCardsWidget);
                QPoint relPos = endPos - m_tableCardsWidget->mapTo(this, QPoint(0, 0));
                cw->move(relPos);
                cw->show();
                cw->setFlying(false);
                cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                m_tableCardWidgets.push_back(cw);
            });

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

// ── 控件抖动 ──────────────────────────────────────────────────

void MainWindow::shakeWidget(QWidget* widget)
{
    if (!widget || m_shaking) return;
    m_shaking = true;

    QPoint orig = widget->pos();
    QTimer* timer = new QTimer(this);
    int* counter = new int(0);

    connect(timer, &QTimer::timeout, this, [=]() mutable {
        if (*counter >= 8) {
            timer->stop();
            widget->move(orig);
            delete counter;
            timer->deleteLater();
            m_shaking = false;
            return;
        }
        int dx = ((*counter) % 2 == 0) ? 4 : -4;
        widget->move(orig.x() + dx, orig.y());
        (*counter)++;
    });
    timer->start(30);
}

// ── 特殊胜利特效 ──────────────────────────────────────────────────

void MainWindow::showSpecialVictoryEffect(const QString& winnerName, const QString& endMessage)
{
    m_gameOver = true;
    disableActionButtons();
    QWidget* central = centralWidget();

    // 1. 金色光晕覆盖层
    QWidget* glow = new QWidget(central);
    glow->setAttribute(Qt::WA_TransparentForMouseEvents);
    glow->setGeometry(central->rect());
    glow->setStyleSheet("background-color: rgba(255, 215, 0, 50);");
    glow->show();
    glow->raise();

    // 光晕脉冲
    QTimer* pulseTimer = new QTimer(this);
    int* pulseCnt = new int(0);
    connect(pulseTimer, &QTimer::timeout, this, [glow, pulseCnt]() {
        (*pulseCnt)++;
        int a = (*pulseCnt) % 2 == 0 ? 30 : 70;
        glow->setStyleSheet(
            QString("background-color: rgba(255, 215, 0, %1);").arg(a));
    });
    pulseTimer->start(300);

    // 2. 桌面牌放大到 1.5 倍
    for (CardWidget* cw : m_tableCardWidgets) {
        QRect g = cw->geometry();
        QRect big(g.x() - g.width() / 4, g.y() - g.height() / 4,
                  g.width() * 3 / 2, g.height() * 3 / 2);
        QPropertyAnimation* a = new QPropertyAnimation(cw, "geometry");
        a->setDuration(400);
        a->setEasingCurve(QEasingCurve::OutBack);
        a->setEndValue(big);
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // 3. 大字 "七鬼523！"
    QLabel* bigText = new QLabel("七鬼523！", central);
    bigText->setAlignment(Qt::AlignCenter);
    bigText->setStyleSheet(
        "QLabel { color: #FFD700; font-size: 64px; font-weight: bold; background: transparent; }");
    bigText->setAttribute(Qt::WA_TransparentForMouseEvents);
    bigText->setGeometry(0, central->height() / 3, central->width(), 100);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect;
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 200));
    shadow->setOffset(4, 4);
    bigText->setGraphicsEffect(shadow);
    bigText->show();
    bigText->raise();

    // 大字弹出动画
    bigText->resize(0, 0);
    QRect origRect = bigText->geometry();
    QPropertyAnimation* textPop = new QPropertyAnimation(bigText, "geometry");
    textPop->setDuration(500);
    textPop->setEasingCurve(QEasingCurve::OutElastic);
    textPop->setStartValue(QRect(central->width() / 2, central->height() / 3, 0, 0));
    textPop->setEndValue(QRect(0, central->height() / 3, central->width(), 100));
    textPop->start(QAbstractAnimation::DeleteWhenStopped);

    // 4. 1.5 秒后清理并弹对话框
    QTimer::singleShot(1500, this, [this, endMessage, glow, bigText, pulseTimer, pulseCnt, shadow]() {
        pulseTimer->stop();
        delete pulseTimer;
        delete pulseCnt;
        glow->deleteLater();
        bigText->deleteLater();
        shadow->deleteLater();
        showGameOverDialog(endMessage);
    });
}

// ── 压分飘字 ──

void MainWindow::showBonusFloat(int bonus) {
    if (bonus <= 0) return;
    if (!m_tableScoreLabel) return;

    QLabel* floatLabel = new QLabel(QString("+%1").arg(bonus), this);
    floatLabel->setStyleSheet(
        "QLabel {"
        "  color: #FFD700;"
        "  font-size: 32px;"
        "  font-weight: bold;"
        "  background: transparent;"
        "}"
    );
    floatLabel->setAlignment(Qt::AlignCenter);
    floatLabel->adjustSize();

    QPoint labelPos = m_tableScoreLabel->mapTo(this, QPoint(0, 0));
    int x = labelPos.x() + m_tableScoreLabel->width() / 2 - floatLabel->width() / 2;
    int y = labelPos.y() - floatLabel->height() - 5;
    floatLabel->move(x, y);
    floatLabel->show();
    floatLabel->raise();

    // 向上飘 + 淡出
    QPropertyAnimation* moveAnim = new QPropertyAnimation(floatLabel, "pos");
    moveAnim->setDuration(1000);
    moveAnim->setStartValue(QPoint(x, y));
    moveAnim->setEndValue(QPoint(x, y - 80));
    moveAnim->setEasingCurve(QEasingCurve::OutCubic);

    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(floatLabel);
    floatLabel->setGraphicsEffect(effect);
    QPropertyAnimation* fadeAnim = new QPropertyAnimation(effect, "opacity");
    fadeAnim->setDuration(1000);
    fadeAnim->setStartValue(1.0);
    fadeAnim->setEndValue(0.0);

    connect(moveAnim, &QPropertyAnimation::finished, floatLabel, &QLabel::deleteLater);
    moveAnim->start(QAbstractAnimation::DeleteWhenStopped);
    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── 桌面牌布局 ──

void MainWindow::layoutTableCards() {
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

// ── 窗口大小变化 ──

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    layoutTableCards();
}