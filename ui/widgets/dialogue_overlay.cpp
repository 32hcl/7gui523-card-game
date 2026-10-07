#include "dialogue_overlay.h"
#include "ui/widgets/boss_avatar.h"
#include "ui/audio/audio_manager.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDebug>

DialogueOverlay::DialogueOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    hide();

    // 打字机定时器
    m_typeTimer = new QTimer(this);
    m_typeTimer->setInterval(m_typeSpeedMs);
    connect(m_typeTimer, &QTimer::timeout, this, &DialogueOverlay::onTypewriterTick);
}

void DialogueOverlay::playDialogue(const QVector<DialogueLine>& lines, int bossLevel)
{
    if (lines.isEmpty()) {
        emit dialogueFinished();
        return;
    }

    m_lines = lines;
    m_bossLevel = bossLevel;
    m_currentIdx = 0;
    m_isPlaying = true;

    show();
    raise();
    setFocus();

    showCurrentLine();
}

void DialogueOverlay::skipAll()
{
    if (!m_isPlaying) return;

    // 停止打字机，直接显示当前句完整文字
    if (m_typeTimer->isActive()) {
        finishTypewriter();
        return;  // 第一次点击：完成当前句
    }

    // 已经显示完整，跳到最后一句并结束
    m_currentIdx = m_lines.size() - 1;
    showCurrentLine();
    finishTypewriter();
    m_isPlaying = false;
    hide();
    emit dialogueFinished();
}

void DialogueOverlay::advanceLine()
{
    m_currentIdx++;

    if (m_currentIdx >= m_lines.size()) {
        // 全部结束
        m_isPlaying = false;
        hide();
        emit dialogueFinished();
        return;
    }

    showCurrentLine();
}

void DialogueOverlay::showCurrentLine()
{
    if (m_currentIdx >= m_lines.size()) return;

    const DialogueLine& line = m_lines[m_currentIdx];

    if (line.isBoss) {
        // Boss对话模式
        QString bossName = QString::fromUtf8("");
        // 从ai_levels.h获取名字 - 通过bossLevel
        setupBossMode("", line.text);
    } else {
        // 旁白模式
        setupNarrationMode(line.text);
    }

    // 开始打字机效果
    startTypewriter(line.text);
}

void DialogueOverlay::startTypewriter(const QString& text)
{
    m_fullText = text;
    m_typePos = 0;
    m_textLabel->setText(QString());
    m_continueLabel->hide();
    m_typeTimer->start();
}

void DialogueOverlay::onTypewriterTick()
{
    // 处理换行符，逐字显示
    if (m_typePos < m_fullText.length()) {
        // 一次显示一个字符
        m_typePos++;
        m_textLabel->setText(m_fullText.left(m_typePos));

        // 每显示几个字播放一个轻微的打字音效（低音量）
        if (m_typePos % 3 == 0) {
            AUDIO_MGR.playSfxByName("sfx_type.wav");  // 可选，没有也不影响
        }
    } else {
        finishTypewriter();
    }
}

void DialogueOverlay::finishTypewriter()
{
    m_typeTimer->stop();
    m_textLabel->setText(m_fullText);
    m_continueLabel->show();
}

void DialogueOverlay::setupNarrationMode(const QString& text)
{
    Q_UNUSED(text);
    clearLayout();

    // 旁白模式：居中半透明黑底 + 白色文字
    m_dialogBox = new QWidget(this);
    m_dialogBox->setObjectName("narrationBox");
    m_dialogBox->setAttribute(Qt::WA_TranslucentBackground);

    auto* boxLayout = new QVBoxLayout(m_dialogBox);
    boxLayout->setContentsMargins(60, 40, 60, 40);
    boxLayout->setSpacing(20);
    boxLayout->setAlignment(Qt::AlignCenter);

    m_textLabel = new QLabel(m_dialogBox);
    m_textLabel->setObjectName("narrationText");
    m_textLabel->setAlignment(Qt::AlignCenter);
    m_textLabel->setWordWrap(true);
    m_textLabel->setMinimumWidth(600);

    m_continueLabel = new QLabel("▼ 点击继续", m_dialogBox);
    m_continueLabel->setObjectName("continueHint");
    m_continueLabel->setAlignment(Qt::AlignCenter);
    m_continueLabel->hide();

    boxLayout->addWidget(m_textLabel);
    boxLayout->addWidget(m_continueLabel);

    // 居中显示
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addStretch();
    outerLayout->addWidget(m_dialogBox, 0, Qt::AlignHCenter);
    outerLayout->addStretch();

    m_mainLayout = outerLayout;

    m_dialogBox->setStyleSheet(R"(
        #narrationBox {
            background: rgba(0, 0, 0, 0.85);
            border: 1px solid #ffd700;
            border-radius: 12px;
        }
        #narrationText {
            color: #fff8e7;
            font-size: 18px;
            font-family: "Microsoft YaHei", "SimHei", sans-serif;
            line-height: 1.8;
            letter-spacing: 1px;
        }
        #continueHint {
            color: #ffd700;
            font-size: 12px;
            animation: pulse 1.2s infinite;
        }
    )");
}

void DialogueOverlay::setupBossMode(const QString& bossName, const QString& text)
{
    Q_UNUSED(bossName);
    Q_UNUSED(text);
    clearLayout();

    // Boss对话模式：顶部区域，左侧头像，右侧气泡
    m_dialogBox = new QWidget(this);
    m_dialogBox->setObjectName("bossDialogBox");
    m_dialogBox->setAttribute(Qt::WA_TranslucentBackground);

    auto* boxLayout = new QHBoxLayout(m_dialogBox);
    boxLayout->setContentsMargins(40, 30, 40, 30);
    boxLayout->setSpacing(24);

    // 左侧：Boss头像
    m_bossAvatar = new BossAvatar(m_bossLevel, m_dialogBox);
    m_bossAvatar->setAvatarSize(80);

    // 右侧：说话人名字 + 对话内容
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(8);

    m_speakerLabel = new QLabel(m_dialogBox);
    m_speakerLabel->setObjectName("bossSpeakerName");

    // 从对话引擎获取Boss名字和称号
    QString title = DialogueEngine::instance().getBossTitle(m_bossLevel);
    // 获取Boss名 - 通过getBossName但在.h中是inline，我们直接用对话引擎
    const char* rawName = "";
    switch (m_bossLevel) {
        case 1: rawName = "新手"; break;
        case 2: rawName = "换牌虫"; break;
        case 3: rawName = "急眼雀"; break;
        case 4: rawName = "疯狗"; break;
        case 5: rawName = "赖账鬼"; break;
        case 6: rawName = "不识数"; break;
        case 7: rawName = "黑老大"; break;
        case 8: rawName = "千里眼"; break;
        case 9: rawName = "赌场老板"; break;
        default: rawName = "未知"; break;
    }
    m_speakerLabel->setText(QString("%1  ·  %2").arg(title).arg(QString::fromUtf8(rawName)));

    m_textLabel = new QLabel(m_dialogBox);
    m_textLabel->setObjectName("bossDialogText");
    m_textLabel->setWordWrap(true);
    m_textLabel->setMinimumWidth(500);

    m_continueLabel = new QLabel("▼ 点击继续", m_dialogBox);
    m_continueLabel->setObjectName("continueHint");
    m_continueLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    m_continueLabel->hide();

    textLayout->addWidget(m_speakerLabel);
    textLayout->addWidget(m_textLabel);
    textLayout->addWidget(m_continueLabel);

    boxLayout->addWidget(m_bossAvatar, 0, Qt::AlignTop);
    boxLayout->addLayout(textLayout, 1);

    // 放置在顶部
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(60, 80, 60, 0);
    outerLayout->addWidget(m_dialogBox);
    outerLayout->addStretch();

    m_mainLayout = outerLayout;

    m_dialogBox->setStyleSheet(R"(
        #bossDialogBox {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 rgba(26, 15, 10, 230),
                stop:1 rgba(42, 24, 16, 230));
            border: 2px solid #ffd700;
            border-radius: 16px;
            min-height: 120px;
        }
        #bossSpeakerName {
            color: #ffd700;
            font-size: 16px;
            font-weight: bold;
            font-family: "Microsoft YaHei", sans-serif;
            letter-spacing: 1px;
        }
        #bossDialogText {
            color: #fff8e7;
            font-size: 16px;
            font-family: "Microsoft YaHei", sans-serif;
            line-height: 1.7;
        }
        #continueHint {
            color: #ffd700;
            font-size: 12px;
        }
    )");
}

void DialogueOverlay::clearLayout()
{
    // 移除所有子控件
    if (layout()) {
        QLayoutItem* item;
        while ((item = layout()->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete layout();
    }

    m_dialogBox = nullptr;
    m_textLabel = nullptr;
    m_speakerLabel = nullptr;
    m_continueLabel = nullptr;
    m_bossAvatar = nullptr;
    m_mainLayout = nullptr;
}

void DialogueOverlay::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    if (!m_isPlaying) return;

    if (m_typeTimer->isActive()) {
        // 正在打字：直接显示完整
        finishTypewriter();
    } else {
        // 已显示完：进入下一句
        advanceLine();
    }
}

void DialogueOverlay::keyPressEvent(QKeyEvent* event)
{
    if (!m_isPlaying) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_typeTimer->isActive()) {
            finishTypewriter();
        } else {
            advanceLine();
        }
    } else if (event->key() == Qt::Key_Escape) {
        skipAll();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void DialogueOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    // 半透明黑色遮罩
    p.fillRect(rect(), QColor(0, 0, 0, 180));
}

void DialogueOverlay::showEvent(QShowEvent* event)
{
    Q_UNUSED(event);
    // 淡入动画
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(0.0);
    setGraphicsEffect(effect);

    QPropertyAnimation* anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(300);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
