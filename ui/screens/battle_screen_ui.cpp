#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QFile>
#include <QCoreApplication>
#include <QPixmap>

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
    m_tableCardsWidget->setFixedHeight(90);
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
    m_deckDisplayLayout->setContentsMargins(10, 10, 10, 10);
    m_deckDisplayLayout->setSpacing(8);

    // 双牌堆 - 竖向排列
    auto* decksLay = new QVBoxLayout;
    decksLay->setSpacing(24);
    decksLay->setAlignment(Qt::AlignCenter);

    // 玩家牌堆（上）
    m_playerDeckWidget = new QWidget;
    auto* playerDeckLay = new QVBoxLayout(m_playerDeckWidget);
    playerDeckLay->setContentsMargins(0, 0, 0, 0);
    playerDeckLay->setSpacing(4);
    playerDeckLay->setAlignment(Qt::AlignCenter);

    m_playerDeckLabel = new QLabel("玩家牌堆");
    m_playerDeckLabel->setAlignment(Qt::AlignCenter);
    m_playerDeckLabel->setStyleSheet("QLabel { color: #4CAF50; font-size: 12px; font-weight: bold; }");
    playerDeckLay->addWidget(m_playerDeckLabel);

    // 玩家牌堆背图
    {
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            QPixmap backPix(backPath);
            QPixmap scaled = backPix.scaled(60, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QLabel* playerDeckImg = new QLabel;
            playerDeckImg->setFixedSize(64, 94);
            playerDeckImg->setAlignment(Qt::AlignCenter);
            playerDeckImg->setPixmap(scaled);
            playerDeckLay->addWidget(playerDeckImg, 0, Qt::AlignCenter);
        }
    }

    m_playerDeckCountLabel = new QLabel("27 张");
    m_playerDeckCountLabel->setAlignment(Qt::AlignCenter);
    m_playerDeckCountLabel->setStyleSheet("QLabel { color: #81C784; font-size: 13px; font-weight: bold; }");
    playerDeckLay->addWidget(m_playerDeckCountLabel);

    decksLay->addWidget(m_playerDeckWidget);

    // Boss 牌堆（下）
    m_bossDeckWidget = new QWidget;
    auto* bossDeckLay = new QVBoxLayout(m_bossDeckWidget);
    bossDeckLay->setContentsMargins(0, 0, 0, 0);
    bossDeckLay->setSpacing(4);
    bossDeckLay->setAlignment(Qt::AlignCenter);

    m_bossDeckLabel = new QLabel("Boss 牌堆");
    m_bossDeckLabel->setAlignment(Qt::AlignCenter);
    m_bossDeckLabel->setStyleSheet("QLabel { color: #EF5350; font-size: 12px; font-weight: bold; }");
    bossDeckLay->addWidget(m_bossDeckLabel);

    // Boss 牌堆背图
    {
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            QPixmap backPix(backPath);
            QPixmap scaled = backPix.scaled(60, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QLabel* bossDeckImg = new QLabel;
            bossDeckImg->setFixedSize(64, 94);
            bossDeckImg->setAlignment(Qt::AlignCenter);
            bossDeckImg->setPixmap(scaled);
            bossDeckLay->addWidget(bossDeckImg, 0, Qt::AlignCenter);
        }
    }

    m_bossDeckCountLabel = new QLabel("27 张");
    m_bossDeckCountLabel->setAlignment(Qt::AlignCenter);
    m_bossDeckCountLabel->setStyleSheet("QLabel { color: #E57373; font-size: 13px; font-weight: bold; }");
    bossDeckLay->addWidget(m_bossDeckCountLabel);

    decksLay->addWidget(m_bossDeckWidget);

    m_deckDisplayLayout->addLayout(decksLay);
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
    btnGrid->addWidget(m_newGameButton,    2, 0);
    m_returnMenuButton = new QPushButton("返回菜单");
    m_returnMenuButton->setMinimumHeight(34);
    applyBtnShadow(m_returnMenuButton);
    btnGrid->addWidget(m_returnMenuButton, 2, 1);

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
    connect(m_returnMenuButton, &QPushButton::clicked, this, &BattleScreen::onReturnMenuClicked);
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