#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
#include "ui/widgets/boss_avatar.h"
#include "ui/widgets/play_effect_overlay.h"
#include "ui/widgets/dialogue_overlay.h"
#include "ai/ai_levels.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QFile>
#include <QCoreApplication>
#include <QPixmap>

void BattleScreen::createTopBar(QVBoxLayout* leftLay)
{
    auto* topBar = new QWidget;
    topBar->setFixedHeight(50);
    topBar->setStyleSheet(R"(
        QWidget {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(42,20,10,0.95),
                stop:1 rgba(26,15,10,0.9));
            border: 1px solid rgba(255,215,0,0.3);
            border-radius: 8px;
        }
    )");
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(20, 0, 20, 0);

    m_titleLabel = new QLabel("练习模式");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("QLabel { color: #ffd700; }");

    m_deckCountLabel = new QLabel("牌堆");
    m_roundLabel = new QLabel("回合: 1");
    QFont infoFont = m_deckCountLabel->font();
    infoFont.setPointSize(14);
    infoFont.setBold(true);
    m_deckCountLabel->setFont(infoFont);
    m_roundLabel->setFont(infoFont);
    m_deckCountLabel->setStyleSheet("QLabel { color: #ffe66d; }");
    m_roundLabel->setStyleSheet("QLabel { color: #ffe66d; }");

    topLay->addWidget(m_titleLabel);
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
    auto* wrapper = new QWidget;
    wrapper->setStyleSheet(R"(
        QWidget {
            background-color: rgba(42, 15, 20, 0.5);
            border: 1px solid rgba(196,30,58,0.3);
            border-radius: 10px;
        }
    )");
    auto* rowLay = new QHBoxLayout(wrapper);
    rowLay->setContentsMargins(8, 6, 6, 6);
    rowLay->setSpacing(8);

    // Boss 头像+名字（左侧，关卡模式显示）
    m_bossAvatarWidget = new QWidget;
    m_bossAvatarWidget->setFixedWidth(80);
    auto* avatarLay = new QVBoxLayout(m_bossAvatarWidget);
    avatarLay->setContentsMargins(0, 0, 0, 0);
    avatarLay->setSpacing(4);
    avatarLay->setAlignment(Qt::AlignCenter);

    m_bossAvatar = new BossAvatar(1);
    m_bossAvatar->setAvatarSize(56);
    avatarLay->addWidget(m_bossAvatar, 0, Qt::AlignCenter);

    auto* bossNameLabel = new QLabel("对手");
    bossNameLabel->setAlignment(Qt::AlignCenter);
    bossNameLabel->setObjectName("bossNameLabel");
    bossNameLabel->setStyleSheet(R"(
        QLabel#bossNameLabel {
            color: #ffe66d;
            font-size: 11px;
            font-weight: bold;
            background: transparent;
        }
    )");
    avatarLay->addWidget(bossNameLabel);

    rowLay->addWidget(m_bossAvatarWidget);

    m_playerBHandWidget = new QWidget;
    m_playerBHandWidget->setFixedHeight(110);
    rowLay->addWidget(m_playerBHandWidget, 1);

    // Boss 牌堆（右侧）
    m_bossDeckWidget = new QWidget;
    m_bossDeckWidget->setFixedWidth(90);
    auto* bossDeckLay = new QVBoxLayout(m_bossDeckWidget);
    bossDeckLay->setContentsMargins(0, 0, 0, 0);
    bossDeckLay->setSpacing(2);
    bossDeckLay->setAlignment(Qt::AlignCenter);
    {
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            QPixmap backPix(backPath);
            QPixmap scaled = backPix.scaled(60, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            auto* img = new QLabel;
            img->setFixedSize(64, 94);
            img->setAlignment(Qt::AlignCenter);
            img->setPixmap(scaled);
            bossDeckLay->addWidget(img, 0, Qt::AlignCenter);
        }
    }
    m_bossDeckCountLabel = new QLabel("27 张");
    m_bossDeckCountLabel->setAlignment(Qt::AlignCenter);
    m_bossDeckCountLabel->setStyleSheet("QLabel { color: #B0BEC5; font-size: 12px; font-weight: bold; }");
    bossDeckLay->addWidget(m_bossDeckCountLabel);
    rowLay->addWidget(m_bossDeckWidget);

    leftLay->addWidget(wrapper);
    wrapper->setFixedHeight(110);
}

void BattleScreen::createTableArea(QVBoxLayout* leftLay)
{
    // 椭圆形赌桌容器（带桌沿立体感 + 呢绒绿色 + 金边装饰）
    m_tableFrame = new QFrame;
    m_tableFrame->setObjectName("casinoTable");
    m_tableFrame->setStyleSheet(R"(
        #casinoTable {
            background-color: qradial-gradient(cx:0.5, cy:0.4, radius:0.7, fx:0.5, fy:0.3,
                stop:0 #1a8a42,
                stop:0.4 #0d6b33,
                stop:0.75 #0a5025,
                stop:1 #073d1a);
            border: 16px solid transparent;
            border-image: none;
            border-radius: 80px;
        }
    )");

    // 用 QGraphicsDropShadowEffect 做外发光金边
    QGraphicsDropShadowEffect* tableGlow = new QGraphicsDropShadowEffect(m_tableFrame);
    tableGlow->setBlurRadius(18);
    tableGlow->setOffset(0, 0);
    tableGlow->setColor(QColor(255, 215, 0, 160));
    m_tableFrame->setGraphicsEffect(tableGlow);

    auto* tableLay = new QVBoxLayout(m_tableFrame);
    tableLay->setContentsMargins(40, 24, 40, 24);
    tableLay->setSpacing(8);

    // 桌面中央：牌区 + 提示
    // 桌面卡片区
    m_tableCardsWidget = new QWidget;
    m_tableCardsWidget->setAttribute(Qt::WA_TranslucentBackground, true);
    tableLay->addWidget(m_tableCardsWidget, 1);

    // 桌面中央水印（覆盖在桌面中央，透明鼠标穿透）
    m_tableWatermark = new QLabel("♠ 鸿运赌场 ♥", m_tableFrame);
    m_tableWatermark->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_tableWatermark->setStyleSheet(R"(
        QLabel {
            color: rgba(255, 215, 0, 0.08);
            font-size: 56px;
            font-weight: bold;
            font-family: "KaiTi", "STKaiti", "Microsoft YaHei", serif;
            letter-spacing: 12px;
            background: transparent;
        }
    )");
    m_tableWatermark->setAlignment(Qt::AlignCenter);
    m_tableWatermark->setGeometry(0, 0, 500, 80);
    m_tableWatermark->show();
    m_tableWatermark->lower();

    m_tableHintLabel = new QLabel("等待出牌", m_tableCardsWidget);
    m_tableHintLabel->setAlignment(Qt::AlignCenter);
    m_tableHintLabel->setStyleSheet(R"(
        QLabel {
            color: rgba(255, 215, 0, 0.5);
            font-size: 20px;
            font-weight: bold;
            letter-spacing: 4px;
        }
    )");
    QFont hintFont = m_tableHintLabel->font();
    hintFont.setPointSize(16);
    m_tableHintLabel->setFont(hintFont);
    m_tableHintLabel->hide();

    // 底行：上一手牌型 + 桌面分
    auto* infoWidget = new QWidget;
    infoWidget->setFixedHeight(32);
    infoWidget->setAttribute(Qt::WA_TranslucentBackground, true);
    auto* infoRow = new QHBoxLayout(infoWidget);
    infoRow->setContentsMargins(0, 0, 0, 0);
    infoRow->setSpacing(8);

    m_handTypeLabel = new QLabel("上一手牌型: 无");
    QFont tf = m_handTypeLabel->font();
    tf.setPointSize(11);
    m_handTypeLabel->setFont(tf);
    m_handTypeLabel->setStyleSheet("QLabel { color: rgba(255, 248, 231, 0.7); }");
    m_handTypeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    infoRow->addWidget(m_handTypeLabel);

    m_tableScoreLabel = new QLabel("原始分: 0 分 | 奖励分: 0 分 | 合计: 0 分");
    QFont tableFont = m_tableScoreLabel->font();
    tableFont.setPointSize(11);
    tableFont.setBold(true);
    m_tableScoreLabel->setFont(tableFont);
    m_tableScoreLabel->setStyleSheet("QLabel { color: #ffd700; }");
    m_tableScoreLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    infoRow->addWidget(m_tableScoreLabel, 1);

    tableLay->addWidget(infoWidget);

    // 出牌特效层（覆盖在桌面卡片区）
    m_effectOverlay = new PlayEffectOverlay(m_tableCardsWidget);
    m_effectOverlay->hide();

    leftLay->addWidget(m_tableFrame, 2);  // 桌面占2份比例
}

void BattleScreen::createPlayerArea(QVBoxLayout* leftLay)
{
    auto* wrapper = new QWidget;
    wrapper->setStyleSheet(R"(
        QWidget {
            background-color: rgba(42, 26, 20, 0.6);
            border: 1px solid rgba(255,215,0,0.3);
            border-radius: 10px;
        }
    )");
    auto* rowLay = new QHBoxLayout(wrapper);
    rowLay->setContentsMargins(6, 4, 6, 4);
    rowLay->setSpacing(6);

    m_playerAHandWidget = new QWidget;
    m_playerAHandWidget->setFixedHeight(190);
    rowLay->addWidget(m_playerAHandWidget, 1);

    // 玩家牌堆（右侧）
    m_playerDeckWidget = new QWidget;
    m_playerDeckWidget->setFixedWidth(90);
    auto* playerDeckLay = new QVBoxLayout(m_playerDeckWidget);
    playerDeckLay->setContentsMargins(0, 0, 0, 0);
    playerDeckLay->setSpacing(2);
    playerDeckLay->setAlignment(Qt::AlignCenter);
    {
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            QPixmap backPix(backPath);
            QPixmap scaled = backPix.scaled(60, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            auto* img = new QLabel;
            img->setFixedSize(64, 94);
            img->setAlignment(Qt::AlignCenter);
            img->setPixmap(scaled);
            playerDeckLay->addWidget(img, 0, Qt::AlignCenter);
        }
    }
    m_playerDeckCountLabel = new QLabel("27 张");
    m_playerDeckCountLabel->setAlignment(Qt::AlignCenter);
    m_playerDeckCountLabel->setStyleSheet("QLabel { color: #B0BEC5; font-size: 12px; font-weight: bold; }");
    playerDeckLay->addWidget(m_playerDeckCountLabel);
    rowLay->addWidget(m_playerDeckWidget);

    leftLay->addWidget(wrapper);
    wrapper->setFixedHeight(190);
}

QWidget* BattleScreen::createBottomBar()
{
    QWidget* bottomBar = new QWidget;
    auto* bottomLay = new QVBoxLayout(bottomBar);
    bottomLay->setContentsMargins(0, 8, 0, 0);
    bottomLay->setSpacing(14);

    auto* scoreRow = new QHBoxLayout;
    m_scoreALabel = new QLabel("玩家A: 0 分");
    m_scoreBLabel = new QLabel("电脑: 0 分");
    m_scoreALabel->setStyleSheet("QLabel { color: #F5A623; font-size: 14px; font-weight: bold; }");
    m_scoreBLabel->setStyleSheet("QLabel { color: #F5A623; font-size: 14px; font-weight: bold; }");
    scoreRow->addWidget(m_scoreALabel);
    scoreRow->addStretch();
    scoreRow->addWidget(m_scoreBLabel);
    bottomLay->addLayout(scoreRow);

    m_hpLabel = new QLabel("");
    m_hpLabel->setStyleSheet("QLabel { color: #FFAB40; font-size: 13px; font-weight: bold; }");
    m_hpLabel->setAlignment(Qt::AlignCenter);
    bottomLay->addWidget(m_hpLabel);

    // 筹码显示（关卡模式）
    m_chipsLabel = new QLabel("筹码: 100");
    m_chipsLabel->setStyleSheet(R"(
        QLabel {
            color: #ffe66d;
            font-size: 13px;
            font-weight: bold;
            background-color: rgba(42,26,20,0.6);
            border: 1px solid rgba(255,215,0,0.25);
            border-radius: 8px;
            padding: 4px 10px;
        }
    )");
    m_chipsLabel->setAlignment(Qt::AlignCenter);
    m_chipsLabel->hide(); // 练习模式隐藏
    bottomLay->addWidget(m_chipsLabel, 0, Qt::AlignCenter);

    m_entryChipsLabel = new QLabel("");
    m_entryChipsLabel->setStyleSheet("QLabel { color: #c9b896; font-size: 11px; }");
    m_entryChipsLabel->setAlignment(Qt::AlignCenter);
    m_entryChipsLabel->hide();
    bottomLay->addWidget(m_entryChipsLabel, 0, Qt::AlignCenter);

    auto* btnGrid = new QGridLayout;
    btnGrid->setSpacing(14);

    m_playButton    = new QPushButton("出牌");
    m_hpSwapButton  = new QPushButton("扣血换牌");
    m_passButton    = new QPushButton("不要");
    m_pickButton    = new QPushButton("选卡");
    m_difficultyButton = new QPushButton("难度: 电脑1 新手");
    m_newGameButton = new QPushButton("重新开始");
    m_returnMenuButton = new QPushButton("返回菜单");
    m_shopButton    = new QPushButton("商店");

    m_playButton->setMinimumHeight(44);
    m_hpSwapButton->setMinimumHeight(44);
    m_passButton->setMinimumHeight(44);
    m_pickButton->setMinimumHeight(44);
    m_difficultyButton->setMinimumHeight(44);
    m_newGameButton->setMinimumHeight(44);
    m_returnMenuButton->setMinimumHeight(44);
    m_shopButton->setMinimumHeight(44);

    applyBtnShadow(m_playButton);
    applyBtnShadow(m_hpSwapButton);
    applyBtnShadow(m_passButton);
    applyBtnShadow(m_pickButton);
    applyBtnShadow(m_difficultyButton);
    applyBtnShadow(m_newGameButton);
    applyBtnShadow(m_shopButton);

    m_buttonStack = new QStackedWidget;
    m_buttonStack->addWidget(m_passButton);
    m_buttonStack->addWidget(m_pickButton);
    m_buttonStack->setMinimumHeight(40);

    // ── 2行×3列 按钮网格 ──
    // 第0行: 出牌 | 扣血换牌 | 不要/选卡
    btnGrid->addWidget(m_playButton,    0, 0);
    btnGrid->addWidget(m_hpSwapButton,  0, 1);
    btnGrid->addWidget(m_buttonStack,   0, 2);
    // 第1行: 重新开始 | 商店 | 返回菜单
    btnGrid->addWidget(m_newGameButton,    1, 0);
    btnGrid->addWidget(m_shopButton,       1, 1);
    btnGrid->addWidget(m_returnMenuButton, 1, 2);

    bottomLay->addLayout(btnGrid);

    // 难度选择按钮（仅练习模式显示，放在网格下方）
    m_difficultyButton->hide();  // 默认隐藏，练习模式 startNewGame() 会显式显示
    bottomLay->addWidget(m_difficultyButton);

    return bottomBar;
}

void BattleScreen::createRightPanel(QHBoxLayout* rootLayout, QWidget* bottomBar)
{
    auto* rightPanel = new QWidget;
    rightPanel->setObjectName("rightScorePanel");
    rightPanel->setStyleSheet(R"(
        #rightScorePanel {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 rgba(42, 24, 16, 0.9),
                stop:1 rgba(26, 15, 10, 0.95));
            border-left: 2px solid #ffd700;
            border-top: 1px solid rgba(255, 215, 0, 0.3);
            border-bottom: 1px solid rgba(255, 215, 0, 0.3);
            border-right: 1px solid rgba(255, 215, 0, 0.3);
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }
        #rightScorePanel QLabel {
            background-color: transparent;
            border: none;
        }
        #rightScorePanel QTextEdit {
            background-color: rgba(10, 5, 3, 0.6);
            color: #c9b896;
            border: 1px solid rgba(255, 215, 0, 0.2);
            border-radius: 6px;
            padding: 6px;
            font-size: 12px;
            font-family: "Microsoft YaHei";
        }
    )");
    rightPanel->setMaximumWidth(340);
    rightPanel->setMinimumWidth(280);

    auto* rightLay   = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(12, 10, 12, 10);
    rightLay->setSpacing(8);

    auto* logLabel = new QLabel("📋 牌局记录");
    QFont logFont = logLabel->font();
    logFont.setPointSize(14);
    logFont.setBold(true);
    logLabel->setFont(logFont);
    logLabel->setStyleSheet(R"(
        QLabel {
            color: #ffd700;
            letter-spacing: 2px;
        }
    )");
    rightLay->addWidget(logLabel);

    m_logTextEdit = new QTextEdit;
    m_logTextEdit->setReadOnly(true);
    rightLay->addWidget(m_logTextEdit, 1);

    rightLay->addWidget(bottomBar);

    // 主区占3份，右栏占1份
    rootLayout->addWidget(rightPanel, 0, Qt::AlignRight);
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

// 初始化猜牌界面：标题"猜牌决定先手"、猜大鬼/小鬼按钮、牌背显示区域
// 覆盖在桌面区域上，关卡模式每局开始时显示
void BattleScreen::initGuessWidget()
{
    m_guessWidget = new QWidget(m_tableCardsWidget);
    m_guessWidget->setGeometry(0, 0, 400, 300);
    m_guessWidget->setStyleSheet("background: transparent;");

    auto* outer = new QVBoxLayout(m_guessWidget);
    outer->setAlignment(Qt::AlignCenter);
    outer->setSpacing(15);

    auto* title = new QLabel("猜牌决定先手");
    title->setAlignment(Qt::AlignCenter);
    QFont tf = title->font();
    tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet("color: #FFD700; background: transparent;");
    outer->addWidget(title);

    auto* midRow = new QHBoxLayout();
    midRow->setAlignment(Qt::AlignCenter);
    midRow->setSpacing(30);

    auto* btnBig = new QPushButton("猜大鬼");
    btnBig->setObjectName("guessBigBtn");
    auto* btnSmall = new QPushButton("猜小鬼");
    btnSmall->setObjectName("guessSmallBtn");

    for (auto* b : {btnBig, btnSmall}) {
        QFont bf = b->font();
        bf.setPointSize(16); bf.setBold(true);
        b->setFont(bf);
        b->setMinimumSize(120, 50);
        applyBtnShadow(b);
    }
    btnBig->setStyleSheet(btnBig->styleSheet() + "QPushButton { background-color: #C0392B; color: white; border-radius: 8px; }"
                          "QPushButton:hover { background-color: #E74C3C; }");
    btnSmall->setStyleSheet(btnSmall->styleSheet() + "QPushButton { background-color: #2C3E50; color: white; border-radius: 8px; }"
                            "QPushButton:hover { background-color: #34495E; }");

    midRow->addWidget(btnBig);
    midRow->addWidget(btnSmall);
    outer->addLayout(midRow);

    m_guessCardLabel = new QLabel();
    m_guessCardLabel->setFixedSize(100, 140);
    m_guessCardLabel->setAlignment(Qt::AlignCenter);
    m_guessCardLabel->setStyleSheet("background: transparent;");
    m_guessCardLabel->hide();

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(m_guessCardLabel);
    shadow->setBlurRadius(15);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    m_guessCardLabel->setGraphicsEffect(shadow);

    outer->addWidget(m_guessCardLabel, 0, Qt::AlignCenter);

    m_guessWidget->hide();
}

void BattleScreen::connectSignals()
{
    auto* fb = m_firstChoiceWidget->findChild<QPushButton*>("firstBtn");
    auto* rb = m_firstChoiceWidget->findChild<QPushButton*>("randomBtn");
    auto* sb = m_firstChoiceWidget->findChild<QPushButton*>("secondBtn");
    if (fb) connect(fb, &QPushButton::clicked, this, &BattleScreen::onFirstBtnClicked);
    if (rb) connect(rb, &QPushButton::clicked, this, &BattleScreen::onRandomBtnClicked);
    if (sb) connect(sb, &QPushButton::clicked, this, &BattleScreen::onSecondBtnClicked);

    auto* gb = m_guessWidget->findChild<QPushButton*>("guessBigBtn");
    auto* gs = m_guessWidget->findChild<QPushButton*>("guessSmallBtn");
    if (gb) connect(gb, &QPushButton::clicked, this, [this]() { onGuessJoker(true); });
    if (gs) connect(gs, &QPushButton::clicked, this, [this]() { onGuessJoker(false); });

    connect(m_playButton,       &QPushButton::clicked, this, &BattleScreen::onPlayButtonClicked);
    connect(m_passButton,       &QPushButton::clicked, this, &BattleScreen::onPassButtonClicked);
    connect(m_hpSwapButton,     &QPushButton::clicked, this, &BattleScreen::onHpSwapButtonClicked);
    connect(m_pickButton,       &QPushButton::clicked, this, &BattleScreen::onPickButtonClicked);
    connect(m_difficultyButton, &QPushButton::clicked, this, &BattleScreen::onDifficultyButtonClicked);
    connect(m_newGameButton,    &QPushButton::clicked, this, &BattleScreen::onNewGameButtonClicked);
    connect(m_returnMenuButton, &QPushButton::clicked, this, &BattleScreen::onReturnMenuClicked);
    connect(m_shopButton,       &QPushButton::clicked, this, &BattleScreen::onShopButtonClicked);
}

QWidget* BattleScreen::createCardBack()
{
    const int w = 65;
    const int h = 95;

    QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";

    QLabel* back = new QLabel;
    back->setFixedSize(w, h);
    back->setAlignment(Qt::AlignCenter);

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
    if (m_effectOverlay && m_tableCardsWidget) {
        m_effectOverlay->setGeometry(m_tableCardsWidget->rect());
    }
    if (m_dialogueOverlay) {
        m_dialogueOverlay->setGeometry(rect());
        m_dialogueOverlay->raise();
    }
    // 桌面水印居中
    if (m_tableWatermark && m_tableFrame) {
        int w = m_tableFrame->width();
        int h = m_tableFrame->height();
        m_tableWatermark->setGeometry(0, (h - 80) / 2, w, 80);
        m_tableWatermark->lower();
    }
    layoutTableCards();
    layoutHandSlots(true, false);
    layoutHandSlots(false, false);
}

QPoint BattleScreen::handSlotPos(QWidget* handWidget, int index, int cardHeight) const {
    const int cw = 110, gap = 6;
    const int ch = cardHeight;
    const int W = handWidget->width();
    const int H = handWidget->height();
    const int totalW = kMaxHandSize * cw + (kMaxHandSize - 1) * gap;
    const int left = (W - totalW) / 2;
    const int y = (H - ch) / 2;
    return QPoint(left + index * (cw + gap), y);
}

void BattleScreen::layoutHandSlots(bool playerA, bool animate) {
    QWidget* handWidget = playerA ? m_playerAHandWidget : m_playerBHandWidget;
    if (!handWidget) return;

    qDebug() << "[layoutHandSlots] playerA=" << playerA
             << "count=" << (int)(playerA ? m_playerACardWidgets.size() : m_playerBCardWidgets.size())
             << "W=" << handWidget->width()
             << "H=" << handWidget->height()
             << "firstSlot=" << handSlotPos(handWidget, 0);

    if (playerA) {
        for (size_t i = 0; i < m_playerACardWidgets.size(); ++i) {
            CardWidget* cw = m_playerACardWidgets[i];
            QPoint target = handSlotPos(handWidget, static_cast<int>(i));
            if (animate) {
                QPropertyAnimation* a = new QPropertyAnimation(cw, "pos");
                a->setDuration(150);
                a->setEasingCurve(QEasingCurve::OutCubic);
                a->setEndValue(target);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            } else {
                cw->move(target);
            }
        }
    } else {
        for (size_t i = 0; i < m_playerBCardWidgets.size(); ++i) {
            QWidget* w = m_playerBCardWidgets[i];
            QPoint target = handSlotPos(handWidget, static_cast<int>(i), 95);
            if (animate) {
                QPropertyAnimation* a = new QPropertyAnimation(w, "pos");
                a->setDuration(150);
                a->setEasingCurve(QEasingCurve::OutCubic);
                a->setEndValue(target);
                a->start(QAbstractAnimation::DeleteWhenStopped);
            } else {
                w->move(target);
            }
        }
    }
}