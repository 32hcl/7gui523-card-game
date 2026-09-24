#include "battle_screen.h"
#include "ui/widgets/card_widget.h"
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
    topBar->setStyleSheet("QWidget { background-color: #1e1e2a; border-radius: 8px; }");
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(20, 0, 20, 0);

    m_titleLabel = new QLabel("练习模式");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("QLabel { color: #FFD700; }");

    m_deckCountLabel = new QLabel("牌堆");
    m_roundLabel = new QLabel("回合: 1");
    QFont infoFont = m_deckCountLabel->font();
    infoFont.setPointSize(14);
    infoFont.setBold(true);
    m_deckCountLabel->setFont(infoFont);
    m_roundLabel->setFont(infoFont);
    m_deckCountLabel->setStyleSheet("QLabel { color: #F5A623; }");
    m_roundLabel->setStyleSheet("QLabel { color: #F5A623; }");

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
    auto* rowLay = new QHBoxLayout(wrapper);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(6);

    m_playerBHandWidget = new QWidget;
    m_playerBHandWidget->setFixedHeight(190);
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
    m_tableFrame = new QFrame;
    m_tableFrame->setFrameShape(QFrame::StyledPanel);
    m_tableFrame->setStyleSheet(R"(
        QFrame {
            background-color: #1e1e2a;
            border: 1px solid #FFFFFF;
            border-radius: 12px;
        }
    )");
    // no setMinimumHeight — will stretch in leftLay

    QGraphicsDropShadowEffect* tableShadow = new QGraphicsDropShadowEffect(m_tableFrame);
    tableShadow->setBlurRadius(20);
    tableShadow->setOffset(0, 8);
    tableShadow->setColor(QColor(0, 0, 0, 160));
    m_tableFrame->setGraphicsEffect(tableShadow);

    auto* tableLay = new QVBoxLayout(m_tableFrame);
    tableLay->setContentsMargins(12, 6, 12, 6);
    tableLay->setSpacing(4);

    m_tableCardsWidget = new QWidget;
    m_tableCardsWidget->setAttribute(Qt::WA_StyledBackground, true);
    tableLay->addWidget(m_tableCardsWidget, 1);

    m_tableHintLabel = new QLabel("等待出牌", m_tableCardsWidget);
    m_tableHintLabel->setAlignment(Qt::AlignCenter);
    m_tableHintLabel->setStyleSheet("QLabel { color: #F5A623; }");
    QFont hintFont = m_tableHintLabel->font();
    hintFont.setPointSize(16);
    m_tableHintLabel->setFont(hintFont);
    m_tableHintLabel->hide();

    // 底行：上一手牌型 + 桌面分
    auto* infoWidget = new QWidget;
    infoWidget->setFixedHeight(32);
    auto* infoRow = new QHBoxLayout(infoWidget);
    infoRow->setContentsMargins(0, 0, 0, 0);
    infoRow->setSpacing(8);

    m_handTypeLabel = new QLabel("上一手牌型: 无");
    QFont tf = m_handTypeLabel->font();
    tf.setPointSize(11);
    m_handTypeLabel->setFont(tf);
    m_handTypeLabel->setStyleSheet("QLabel { color: #B0BEC5; }");
    m_handTypeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    infoRow->addWidget(m_handTypeLabel);

    m_tableScoreLabel = new QLabel("原始分: 0 分 | 奖励分: 0 分 | 合计: 0 分");
    QFont tableFont = m_tableScoreLabel->font();
    tableFont.setPointSize(11);
    tableFont.setBold(true);
    m_tableScoreLabel->setFont(tableFont);
    m_tableScoreLabel->setStyleSheet("QLabel { color: #FFE082; }");
    m_tableScoreLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    infoRow->addWidget(m_tableScoreLabel, 1);

    tableLay->addWidget(infoWidget);

    leftLay->addWidget(m_tableFrame, 1);
}

void BattleScreen::createPlayerArea(QVBoxLayout* leftLay)
{
    auto* wrapper = new QWidget;
    auto* rowLay = new QHBoxLayout(wrapper);
    rowLay->setContentsMargins(0, 0, 0, 0);
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
    bottomLay->setSpacing(6);

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
    logLabel->setStyleSheet("QLabel { color: #F5A623; }");
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
    layoutHandSlots(true, false);
    layoutHandSlots(false, false);
}

QPoint BattleScreen::handSlotPos(QWidget* handWidget, int index) const {
    const int cw = 110, ch = 180, gap = 6;
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
            QPoint target = handSlotPos(handWidget, static_cast<int>(i));
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