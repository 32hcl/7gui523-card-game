#include "main_menu_screen.h"
#include "ui/audio/audio_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QFont>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>
#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace {

// 从SVG文件渲染为QPixmap
QPixmap loadSvgPixmap(const QString& fileName, int width, int height)
{
    QString svgPath = QCoreApplication::applicationDirPath() + "/svg/" + fileName;
    if (!QFile::exists(svgPath)) {
        // 回退到资源
        svgPath = QStringLiteral(":/svg/") + fileName;
    }

    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid()) {
        // SVG不可用，返回空Pixmap
        QPixmap pm(width, height);
        pm.fill(Qt::transparent);
        return pm;
    }

    QPixmap pm(width, height);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    return pm;
}

} // anonymous namespace

MainMenuScreen::MainMenuScreen(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("MainMenuScreen");

    // ── 整体布局 ──
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(40, 30, 40, 30);
    rootLayout->setSpacing(0);

    // ── 顶部装饰栏：左右红灯笼 ──
    auto* topDeco = new QHBoxLayout();
    topDeco->setContentsMargins(0, 0, 0, 0);

    // 左侧灯笼
    auto* leftLantern = new QLabel();
    leftLantern->setPixmap(loadSvgPixmap("lantern_red.svg", 40, 60));
    leftLantern->setAlignment(Qt::AlignCenter);
    leftLantern->setFixedHeight(60);

    // 右侧灯笼
    auto* rightLantern = new QLabel();
    rightLantern->setPixmap(loadSvgPixmap("lantern_red.svg", 40, 60));
    rightLantern->setAlignment(Qt::AlignCenter);
    rightLantern->setFixedHeight(60);

    // 中间赌场名号
    auto* casinoName = new QLabel("◆ 鸿运赌场 ◆");
    casinoName->setProperty("menuStyle", "casinoName");
    casinoName->setAlignment(Qt::AlignCenter);
    QFont nameFont = casinoName->font();
    nameFont.setPointSize(14);
    nameFont.setBold(true);
    nameFont.setLetterSpacing(QFont::AbsoluteSpacing, 4);
    casinoName->setFont(nameFont);
    casinoName->setStyleSheet(R"(
        color: #ffd700;
        letter-spacing: 4px;
    )");

    topDeco->addWidget(leftLantern, 0, Qt::AlignLeft);
    topDeco->addWidget(casinoName, 1, Qt::AlignCenter);
    topDeco->addWidget(rightLantern, 0, Qt::AlignRight);

    rootLayout->addLayout(topDeco);

    rootLayout->addSpacing(20);

    // ── 标题区 ──
    auto* title = new QLabel("七鬼523");
    title->setProperty("menuStyle", "gameTitle");
    title->setAlignment(Qt::AlignCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(48);
    titleFont.setBold(true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 8);
    title->setFont(titleFont);
    title->setStyleSheet(R"(
        color: #ffd700;
        text-shadow: 0 0 20px rgba(255,215,0,0.5);
    )");
    // 标题阴影
    {
        auto* shadow = new QGraphicsDropShadowEffect(title);
        shadow->setBlurRadius(20);
        shadow->setOffset(0, 2);
        shadow->setColor(QColor(255, 180, 0, 120));
        title->setGraphicsEffect(shadow);
    }
    rootLayout->addWidget(title);

    // 副标题
    auto* subtitle = new QLabel("筹码棋牌 · 斗智斗勇");
    subtitle->setProperty("menuStyle", "gameSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    QFont subFont = subtitle->font();
    subFont.setPointSize(18);
    subFont.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    subtitle->setFont(subFont);
    subtitle->setStyleSheet(R"(
        color: #c9b896;
    )");
    rootLayout->addWidget(subtitle);

    rootLayout->addSpacing(12);

    // 装饰分割线
    auto* decoLine = new QLabel("═══ ◆ ═══");
    decoLine->setAlignment(Qt::AlignCenter);
    decoLine->setStyleSheet("color: rgba(255,215,0,0.4); font-size: 14px; letter-spacing: 4px;");
    rootLayout->addWidget(decoLine);

    rootLayout->addSpacing(30);

    // ── 筹码显示 ──
    auto* chipBadge = new QLabel("🎡  100,000  筹码");
    chipBadge->setProperty("menuStyle", "chipBadge");
    chipBadge->setAlignment(Qt::AlignCenter);
    QFont chipFont = chipBadge->font();
    chipFont.setPointSize(16);
    chipFont.setBold(true);
    chipBadge->setFont(chipFont);
    chipBadge->setStyleSheet(R"(
        color: #ffe66d;
        background-color: rgba(42, 26, 20, 0.8);
        border: 1px solid rgba(255,215,0,0.35);
        border-radius: 20px;
        padding: 8px 24px;
    )");
    rootLayout->addWidget(chipBadge, 0, Qt::AlignCenter);

    rootLayout->addSpacing(30);

    // ── 按钮区 ──
    // 练习模式
    auto* practiceBtn = new QPushButton("练习模式");
    practiceBtn->setProperty("menuStyle", "secondary");
    practiceBtn->setCursor(Qt::PointingHandCursor);
    practiceBtn->setMinimumWidth(320);
    practiceBtn->setMinimumHeight(64);
    QFont btnFont = practiceBtn->font();
    btnFont.setPointSize(20);
    btnFont.setBold(true);
    practiceBtn->setFont(btnFont);
    practiceBtn->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #3c1e14, stop:1 #1e0f0a);
            color: #ffe66d;
            border: 2px solid rgba(255,215,0,0.5);
            border-radius: 14px;
            padding: 14px 40px;
            font-size: 20px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        QPushButton:hover {
            border: 2px solid #ffd700;
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #502819, stop:1 #28150d);
        }
        QPushButton:pressed {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #1e0f0a, stop:1 #3c1e14);
        }
    )");
    {
        auto* shadow = new QGraphicsDropShadowEffect(practiceBtn);
        shadow->setBlurRadius(16);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 160));
        practiceBtn->setGraphicsEffect(shadow);
    }

    auto* practiceDesc = new QLabel("自由选择对手难度与先后手");
    practiceDesc->setAlignment(Qt::AlignCenter);
    practiceDesc->setStyleSheet("color: #8b7355; font-size: 13px;");

    rootLayout->addWidget(practiceBtn, 0, Qt::AlignCenter);
    rootLayout->addWidget(practiceDesc, 0, Qt::AlignCenter);

    rootLayout->addSpacing(20);

    // 关卡模式
    auto* levelBtn = new QPushButton("闯关模式");
    levelBtn->setProperty("menuStyle", "primary");
    levelBtn->setCursor(Qt::PointingHandCursor);
    levelBtn->setMinimumWidth(320);
    levelBtn->setMinimumHeight(64);
    levelBtn->setFont(btnFont);
    levelBtn->setStyleSheet(R"(
        QPushButton {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e63946, stop:0.5 #c41e3a, stop:1 #9b1521);
            color: #fff8e7;
            border: 2px solid #ffd700;
            border-radius: 14px;
            padding: 14px 40px;
            font-size: 20px;
            font-weight: bold;
            letter-spacing: 2px;
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
            padding-top: 16px;
            padding-bottom: 12px;
        }
    )");
    {
        auto* shadow = new QGraphicsDropShadowEffect(levelBtn);
        shadow->setBlurRadius(20);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(196, 30, 58, 120));
        levelBtn->setGraphicsEffect(shadow);
    }

    auto* levelDesc = new QLabel("挑战 9 层赌场高手，赢取筹码步步高升");
    levelDesc->setAlignment(Qt::AlignCenter);
    levelDesc->setStyleSheet("color: #c9b896; font-size: 13px;");

    rootLayout->addWidget(levelBtn, 0, Qt::AlignCenter);
    rootLayout->addWidget(levelDesc, 0, Qt::AlignCenter);

    // 底部弹性空间
    rootLayout->addStretch(1);

    // 底部标语
    auto* footer = new QLabel("★ 鸿运赌场 · 逢赌必赢 ★");
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet(R"(
        color: #8b6508;
        font-size: 12px;
        letter-spacing: 3px;
    )");
    rootLayout->addWidget(footer);

    // ── 连接信号 ──
    connect(practiceBtn, &QPushButton::clicked, this, [this]() {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
        emit practiceModeSelected();
    });
    connect(levelBtn, &QPushButton::clicked, this, [this]() {
        AUDIO_MGR.playSfx(AudioManager::Sfx::Click);
        emit levelModeSelected();
    });

    // ── 播放主菜单BGM ──
    AUDIO_MGR.playBgm(AudioManager::BgmTrack::MainMenu);
}
