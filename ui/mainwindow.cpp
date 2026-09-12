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
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QStackedWidget>
#include <QFile>
#include <QCoreApplication>
#include <QPixmap>
#include <algorithm>

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
        auto* labelB = new QLabel("玩家B");
        QFont playerFontB = labelB->font();
        playerFontB.setPointSize(15);
        playerFontB.setBold(true);
        labelB->setFont(playerFontB);
        labelB->setStyleSheet("QLabel { color: #FFD700; }");
        leftLay->addWidget(labelB);

        m_playerBHandWidget = new QWidget;
        m_playerBLayout     = new QHBoxLayout(m_playerBHandWidget);
        m_playerBLayout->setContentsMargins(0, 0, 0, 0);
        m_playerBLayout->setSpacing(6);
        m_playerBLayout->addStretch();

        leftLay->addWidget(m_playerBHandWidget);
        m_playerBHandWidget->setFixedHeight(120);
    }

    {
        m_tableFrame = new QFrame;
        m_tableFrame->setFrameShape(QFrame::StyledPanel);
        m_tableFrame->setStyleSheet(R"(
            QFrame {
                background-color: #0D3B16;
                border: 3px solid #FFD700;
                border-radius: 12px;
            }
        )");
        m_tableFrame->setMinimumHeight(280);

        auto* tableLay = new QVBoxLayout(m_tableFrame);
        tableLay->setContentsMargins(20, 15, 20, 15);
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
        m_tableCardsLayout = new QHBoxLayout(m_tableCardsWidget);
        m_tableCardsLayout->setContentsMargins(0, 0, 0, 0);
        m_tableCardsLayout->setSpacing(8);
        m_tableCardsLayout->setAlignment(Qt::AlignCenter);
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

        leftLay->addWidget(m_tableFrame);
    }

    {
        auto* labelA = new QLabel("玩家A");
        QFont playerFontA = labelA->font();
        playerFontA.setPointSize(15);
        playerFontA.setBold(true);
        labelA->setFont(playerFontA);
        labelA->setStyleSheet("QLabel { color: #FFD700; }");
        leftLay->addWidget(labelA);

        m_playerAHandWidget = new QWidget;
        m_playerALayout     = new QHBoxLayout(m_playerAHandWidget);
        m_playerALayout->setContentsMargins(0, 0, 0, 0);
        m_playerALayout->setSpacing(6);

        leftLay->addWidget(m_playerAHandWidget);
        m_playerAHandWidget->setFixedHeight(150);
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

        btnGrid->addWidget(m_playButton,       0, 0);
        btnGrid->addWidget(m_buttonStack,      0, 1);
        btnGrid->addWidget(m_difficultyButton, 1, 0, 1, 2);
        btnGrid->addWidget(m_newGameButton,    2, 0, 1, 2);

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

    for (CardWidget* cw : m_playerACardWidgets) {
        if (cw->isSelected()) {
            animateCardToTable(cw);
        }
    }

    // 炸弹/王炸 — 桌面抖动
    if (result.type == CardType::Bomb || result.type == CardType::Rocket) {
        shakeWidget(m_tableFrame);
    }

    for (const Card& c : selected) {
        auto it = std::find_if(m_playerA.hand.begin(), m_playerA.hand.end(),
            [&](const Card& h) {
                return h.point == c.point && h.suit == c.suit;
            });
        if (it != m_playerA.hand.end()) m_playerA.hand.erase(it);
    }

    for (const Card& c : selected) {
        m_tableCards.push_back(c);
    }
    CardTypeResult oldLastPlay = m_lastPlay;
    m_lastPlay = result;
    m_lastPlayerName = "玩家A";

    m_tracker.recordPlayed(selected);

    if (checkSpecialVictory(m_playerA)) {
        appendLog("玩家A 达成七鬼523，直接获胜！");
        playSound(m_soundSuccess);
        showGameOverDialog("玩家A 达成七鬼523，直接获胜！");
        disableActionButtons();
        return;
    }

    int bonus = calculatePressureBonus(result, oldLastPlay);
    if (bonus > 0) {
        m_lastPlay.bonusScore = bonus;
        m_tableBonus += bonus;
    }

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
    QTimer::singleShot(800, this, &MainWindow::doAITurn);
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
            showGameOverDialog("玩家A 达成七鬼523，直接获胜！");
            disableActionButtons();
            return;
        }
        if (checkSpecialVictory(m_playerB)) {
            appendLog("玩家B 达成七鬼523，直接获胜！");
            playSound(m_soundFailure);
            showGameOverDialog("玩家B 达成七鬼523，直接获胜！");
            disableActionButtons();
            return;
        }

        m_waitingForAI = true;
        QTimer::singleShot(800, this, &MainWindow::doAITurn);
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
        showGameOverDialog("玩家A 达成七鬼523，直接获胜！");
        disableActionButtons();
        return;
    }
    if (checkSpecialVictory(m_playerB)) {
        appendLog("玩家B 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showGameOverDialog("玩家B 达成七鬼523，直接获胜！");
        disableActionButtons();
        return;
    }

    m_lastPlay.type = CardType::Invalid;
    m_lastPlay.cards.clear();
    m_lastPlay.keyPoint.clear();
    m_lastPlayerName.clear();
    m_waitingForAI = true;
    QTimer::singleShot(800, this, &MainWindow::doAITurn);
}

void MainWindow::onNewGameButtonClicked()
{
    playSound(m_soundClick);
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
            m_aiLevel = AILevel::AI1_Simple;
            m_difficultyButton->setText("难度: AI1 简单");
            break;
    }
    appendLog(QString("AI 难度切换为: %1").arg(m_difficultyButton->text()));
}

void MainWindow::onPickButtonClicked()
{
    playSound(m_soundClick);
    CardPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        std::vector<Card> selected = dlg.selectedCards();
        if (selected.size() != 5) return;

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
            showGameOverDialog("玩家A 达成七鬼523，直接获胜！");
            disableActionButtons();
            return;
        }
        if (checkSpecialVictory(m_playerB)) {
            appendLog("玩家B 达成七鬼523，直接获胜！");
            playSound(m_soundFailure);
            showGameOverDialog("玩家B 达成七鬼523，直接获胜！");
            disableActionButtons();
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

    if (checkSpecialVictory(m_playerB)) {
        appendLog("玩家B 达成七鬼523，直接获胜！");
        playSound(m_soundFailure);
        showGameOverDialog("玩家B 达成七鬼523，直接获胜！");
        disableActionButtons();
        return;
    }

    int bonus = calculatePressureBonus(m_lastPlay, oldLastPlay);
    if (bonus > 0) {
        m_lastPlay.bonusScore = bonus;
        m_tableBonus += bonus;
    }
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
        QTimer::singleShot(800, this, &MainWindow::doAITurn);
        return;
    }

    enableActionButtons();
    updateUI();
}

void MainWindow::startNewGame()
{
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
    m_roundCount = 1;

    m_logTextEdit->clear();
    appendLog("========== 新游戏开始 ==========");
    appendLog(QString("玩家A 初始手牌: %1").arg(cardsToString(m_playerA.hand)));
    appendLog(QString("玩家B 初始手牌: %1").arg(cardsToString(m_playerB.hand)));
    appendLog("随机先手: 玩家A");
    appendLog(QString("牌堆剩余: %1 张").arg(static_cast<int>(m_deck.cards.size())));

    if (static_cast<int>(m_deck.cards.size()) == 44) {
        m_isPicking = true;
    }

    updateUI();
}

void MainWindow::updateUI()
{
    m_deckCountLabel->setText(
        QString("牌堆剩余: %1").arg(static_cast<int>(m_deck.cards.size())));
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
    while (QLayoutItem* item = m_tableCardsLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    if (m_lastPlay.type == CardType::Invalid || m_lastPlay.cards.empty()) {
        // 桌面为空，显示提示
        QLabel* hint = new QLabel("等待出牌");
        QFont hintFont = hint->font();
        hintFont.setPointSize(16);
        hint->setFont(hintFont);
        hint->setStyleSheet("QLabel { color: #4CAF50; }");
        m_tableCardsLayout->addWidget(hint);
    } else {
        for (const Card& card : m_lastPlay.cards) {
            CardWidget* cw = new CardWidget(card);
            cw->setAttribute(Qt::WA_TransparentForMouseEvents, true);

            auto* effect = new QGraphicsOpacityEffect(cw);
            cw->setGraphicsEffect(effect);
            QPropertyAnimation* fade = new QPropertyAnimation(effect, "opacity");
            fade->setDuration(300);
            fade->setStartValue(0.0);
            fade->setEndValue(1.0);
            fade->start(QAbstractAnimation::DeleteWhenStopped);

            m_tableCardsLayout->addWidget(cw);
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
            m_buttonStack->setCurrentIndex(1);  // 选卡
            m_playButton->setEnabled(!m_playerA.hand.empty());
        } else {
            m_buttonStack->setCurrentIndex(0);  // 不要
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
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("游戏结束");
    msgBox.setText(message);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
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
    const int w = 80;   // 比玩家A 的 100 小
    const int h = 120;

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

// ── 出牌飞行动画 ──────────────────────────────────────────────────

void MainWindow::animateCardToTable(CardWidget* sourceWidget)
{
    if (!sourceWidget) return;

    QPixmap pixmap(sourceWidget->size());
    sourceWidget->render(&pixmap);

    QLabel* flyingCard = new QLabel(this);
    flyingCard->setPixmap(pixmap);
    flyingCard->setFixedSize(sourceWidget->size());

    QPoint startPos = sourceWidget->mapTo(this, QPoint(0, 0));
    QPoint endPos = m_tableCardsWidget->mapTo(this, QPoint(
        m_tableCardsWidget->width() / 2 - sourceWidget->width() / 2,
        m_tableCardsWidget->height() / 2 - sourceWidget->height() / 2));

    flyingCard->move(startPos);
    flyingCard->show();
    flyingCard->raise();

    QPropertyAnimation* anim = new QPropertyAnimation(flyingCard, "pos");
    anim->setDuration(400);
    anim->setStartValue(startPos);
    anim->setEndValue(endPos);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(anim, &QPropertyAnimation::finished, flyingCard, &QLabel::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
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