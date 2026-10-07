#include "level_select_screen.h"
#include "ui/widgets/boss_avatar.h"
#include "dialogue/dialogue_engine.h"
#include "game/campaign.h"
#include "ai/ai_levels.h"
#include "ui/audio/audio_manager.h"

#include <QStyle>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QTimer>

LevelSelectScreen::LevelSelectScreen(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    updateLevelStates();
}

void LevelSelectScreen::setProgress(int clearedLevel)
{
    m_clearedLevel = clearedLevel;
    updateLevelStates();
}

void LevelSelectScreen::setChips(int chips)
{
    m_chips = chips;
    if (m_chipsLabel) {
        m_chipsLabel->setText(QString("🎡 筹码 %1").arg(chips));
    }
}

void LevelSelectScreen::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 顶部栏 ──
    auto* topBar = new QWidget(this);
    topBar->setObjectName("levelSelectTopBar");
    topBar->setFixedHeight(72);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(24, 0, 24, 0);

    m_backButton = new QPushButton("← 返回", this);
    m_backButton->setObjectName("casinoBackBtn");
    m_backButton->setCursor(Qt::PointingHandCursor);
    m_backButton->setFixedSize(80, 38);
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
        emit backToMenu();
    });

    m_titleLabel = new QLabel("鸿运赌场 · 楼层选择", this);
    m_titleLabel->setObjectName("levelSelectTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_chipsLabel = new QLabel(QString("🎡 筹码 %1").arg(m_chips), this);
    m_chipsLabel->setObjectName("levelSelectChips");
    m_chipsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_chipsLabel->setMinimumWidth(120);

    topLayout->addWidget(m_backButton);
    topLayout->addWidget(m_titleLabel, 1);
    topLayout->addWidget(m_chipsLabel);

    mainLayout->addWidget(topBar);

    // ── 楼层列表（滚动区） ──
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("levelScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_levelsWidget = new QWidget();
    m_levelsWidget->setObjectName("levelsContainer");
    m_levelsLayout = new QVBoxLayout(m_levelsWidget);
    m_levelsLayout->setContentsMargins(40, 32, 40, 32);
    m_levelsLayout->setSpacing(20);

    // 从9楼到1楼（上面是高层，下面是低层）
    for (int i = 9; i >= 1; --i) {
        LevelCard lc;
        lc.level = i;

        // 卡片容器
        lc.card = new QWidget(m_levelsWidget);
        lc.card->setObjectName(QString("levelCard_%1").arg(i));
        lc.card->setProperty("level", i);
        lc.card->setCursor(Qt::PointingHandCursor);

        auto* cardLayout = new QHBoxLayout(lc.card);
        cardLayout->setContentsMargins(24, 20, 24, 20);
        cardLayout->setSpacing(20);

        // 左侧：楼层号
        auto* floorLabel = new QLabel(QString("%1F").arg(i), lc.card);
        floorLabel->setObjectName("floorNumber");
        floorLabel->setAlignment(Qt::AlignCenter);
        floorLabel->setFixedWidth(60);

        // Boss头像
        lc.avatar = new BossAvatar(i, lc.card);
        lc.avatar->setAvatarSize(72);

        // 中间：名字 + 简介
        auto* infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(4);

        lc.nameLabel = new QLabel(lc.card);
        lc.nameLabel->setObjectName("bossName");
        {
            QString title = DialogueEngine::instance().getBossTitle(i);
            lc.nameLabel->setText(QString("%1  ·  %2").arg(title).arg(QString::fromUtf8(getBossName(i))));
        }

        lc.titleLabel = new QLabel(DialogueEngine::instance().getBossIntro(i), lc.card);
        lc.titleLabel->setObjectName("bossIntro");
        lc.titleLabel->setWordWrap(true);

        infoLayout->addWidget(lc.nameLabel);
        infoLayout->addWidget(lc.titleLabel);
        infoLayout->addStretch();

        // 右侧：入场筹码 + 开始按钮 + 状态
        auto* rightLayout = new QVBoxLayout();
        rightLayout->setSpacing(8);
        rightLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        int entry = getLevelEntryChips(i);
        int reward = getLevelRewardChips(i);
        lc.chipsLabel = new QLabel(QString("入场 %1 🎡  奖励 %2 🎡").arg(entry).arg(reward), lc.card);
        lc.chipsLabel->setObjectName("chipInfo");
        lc.chipsLabel->setAlignment(Qt::AlignRight);

        lc.startBtn = new QPushButton("挑战", lc.card);
        lc.startBtn->setObjectName("challengeBtn");
        lc.startBtn->setCursor(Qt::PointingHandCursor);
        lc.startBtn->setFixedSize(100, 42);
        connect(lc.startBtn, &QPushButton::clicked, this, [this, i]() { onLevelClicked(i); });

        lc.lockLabel = new QLabel("🔒 未解锁", lc.card);
        lc.lockLabel->setObjectName("lockLabel");
        lc.lockLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lc.lockLabel->hide();

        lc.clearedLabel = new QLabel("✓ 已通关", lc.card);
        lc.clearedLabel->setObjectName("clearedLabel");
        lc.clearedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lc.clearedLabel->hide();

        rightLayout->addWidget(lc.chipsLabel);
        rightLayout->addWidget(lc.startBtn);
        rightLayout->addWidget(lc.lockLabel);
        rightLayout->addWidget(lc.clearedLabel);

        cardLayout->addWidget(floorLabel);
        cardLayout->addWidget(lc.avatar);
        cardLayout->addLayout(infoLayout, 1);
        cardLayout->addLayout(rightLayout);

        // 阴影
        auto* shadow = new QGraphicsDropShadowEffect(lc.card);
        shadow->setBlurRadius(16);
        shadow->setColor(QColor(0, 0, 0, 60));
        shadow->setOffset(0, 4);
        lc.card->setGraphicsEffect(shadow);

        m_levelCards.push_back(lc);
        m_levelsLayout->addWidget(lc.card);
    }

    m_levelsLayout->addStretch();
    m_scrollArea->setWidget(m_levelsWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // 样式
    setStyleSheet(R"(
        #levelSelectTopBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #c41e3a, stop:1 #8b0a22);
            border-bottom: 3px solid #ffd700;
        }
        #levelSelectTitle {
            color: #ffd700;
            font-size: 22px;
            font-weight: bold;
            font-family: "Microsoft YaHei", "SimHei", sans-serif;
            letter-spacing: 4px;
        }
        #levelSelectChips {
            color: #ffd700;
            font-size: 16px;
            font-weight: bold;
            padding: 6px 14px;
            background: rgba(0,0,0,0.3);
            border: 1px solid #ffd700;
            border-radius: 20px;
        }
        #casinoBackBtn {
            color: #fff8e7;
            font-size: 14px;
            background: rgba(0,0,0,0.3);
            border: 1px solid #ffd700;
            border-radius: 19px;
            padding: 6px 14px;
        }
        #casinoBackBtn:hover {
            background: rgba(255, 215, 0, 0.2);
        }
        #levelScrollArea {
            background: qradial-gradient(cx:50% cy:30% radius:70%,
                #1a5c30 0%, #0d5c2a 40%, #083d1c 100%);
        }
        #levelsContainer {
            background: transparent;
        }
        /* 楼层卡片基础样式 */
        #levelScrollArea QWidget[level] {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #2a1810, stop:1 #1a0f0a);
            border: 2px solid #ffd700;
            border-radius: 14px;
            min-height: 112px;
        }
        #levelScrollArea QWidget[level][locked="true"] {
            background: #1a1a1a;
            border: 2px solid #444;
        }
        #levelScrollArea QWidget[level][current="true"] {
            border: 3px solid #ffd700;
        }
        #floorNumber {
            color: #ffd700;
            font-size: 28px;
            font-weight: bold;
            font-family: "Georgia", serif;
        }
        QWidget[locked="true"] #floorNumber {
            color: #555;
        }
        #bossName {
            color: #ffd700;
            font-size: 18px;
            font-weight: bold;
            font-family: "Microsoft YaHei", sans-serif;
        }
        QWidget[locked="true"] #bossName {
            color: #666;
        }
        #bossIntro {
            color: #c9b896;
            font-size: 12px;
            line-height: 1.5;
        }
        QWidget[locked="true"] #bossIntro {
            color: #555;
        }
        #chipInfo {
            color: #ffd700;
            font-size: 12px;
            font-weight: bold;
        }
        QWidget[locked="true"] #chipInfo {
            color: #555;
        }
        #challengeBtn {
            color: #1a0f0a;
            font-size: 15px;
            font-weight: bold;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #ffe066, stop:0.5 #ffd700, stop:1 #b8860b);
            border: 1px solid #b8860b;
            border-radius: 21px;
        }
        #challengeBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #ffed8a, stop:0.5 #ffe066, stop:1 #daa520);
        }
        #challengeBtn:pressed {
            padding-top: 2px;
        }
        #lockLabel {
            color: #888;
            font-size: 14px;
            font-weight: bold;
        }
        #clearedLabel {
            color: #4ade80;
            font-size: 14px;
            font-weight: bold;
        }
        QScrollBar:vertical {
            background: rgba(0,0,0,0.3);
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #ffd700;
            border-radius: 4px;
            min-height: 40px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
    )");
}

void LevelSelectScreen::updateLevelStates()
{
    for (auto& lc : m_levelCards) {
        int level = lc.level;
        bool cleared = level <= m_clearedLevel;
        bool current = level == m_clearedLevel + 1;
        bool locked  = level > m_clearedLevel + 1;

        lc.card->setProperty("locked", locked);
        lc.card->setProperty("current", current);
        lc.card->setCursor(locked ? Qt::ArrowCursor : Qt::PointingHandCursor);

        // 头像置灰
        lc.avatar->setEnabled(!locked);

        // 按钮和状态
        if (cleared) {
            lc.startBtn->show();
            lc.startBtn->setText("再次挑战");
            lc.lockLabel->hide();
            lc.clearedLabel->show();
        } else if (current) {
            lc.startBtn->show();
            lc.startBtn->setText("开始挑战");
            lc.lockLabel->hide();
            lc.clearedLabel->hide();
        } else {
            lc.startBtn->hide();
            lc.lockLabel->show();
            lc.clearedLabel->hide();
        }

        // 重新应用样式（动态属性需要polish）
        lc.card->style()->unpolish(lc.card);
        lc.card->style()->polish(lc.card);
    }
}

void LevelSelectScreen::onLevelClicked(int level)
{
    if (level > m_clearedLevel + 1) return;  // 未解锁
    AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
    emit levelSelected(level);
}
