#include "ui/widgets/play_effect_overlay.h"

#include <QPainter>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QPropertyAnimation>
#include <QTimer>
#include <QtMath>

PlayEffectOverlay::PlayEffectOverlay(QWidget* parent)
    : QWidget(parent)
    , m_effectType(EffectType::None)
    , m_effectProgress(0.0f)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);

    m_anim = new QPropertyAnimation(this, "effectProgress", this);
    m_anim->setStartValue(0.0f);
    m_anim->setEndValue(1.0f);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QPropertyAnimation::finished,
            this, &PlayEffectOverlay::onAnimationFinished);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, [this]() {
        hide();
        m_effectType = EffectType::None;
        emit effectFinished();
    });
}

void PlayEffectOverlay::setEffectProgress(float p)
{
    m_effectProgress = p;
    update();
}

void PlayEffectOverlay::showBombEffect(int durationMs)
{
    startEffect(EffectType::Bomb, durationMs);
}

void PlayEffectOverlay::showRocketEffect(int durationMs)
{
    startEffect(EffectType::Rocket, durationMs);
}

void PlayEffectOverlay::showSpecial523Effect(int durationMs)
{
    startEffect(EffectType::Special523, durationMs);
}

void PlayEffectOverlay::startEffect(EffectType type, int durationMs)
{
    m_effectType = type;
    m_effectProgress = 0.0f;
    show();
    raise();

    m_anim->stop();
    m_anim->setDuration(durationMs);
    m_anim->setStartValue(0.0f);
    m_anim->setEndValue(1.0f);
    m_anim->start();

    m_hideTimer->stop();
    m_hideTimer->start(durationMs + 100);
}

void PlayEffectOverlay::onAnimationFinished()
{
    // 动画播放完毕，等hideTimer触发隐藏
}

void PlayEffectOverlay::paintEvent(QPaintEvent*)
{
    if (m_effectType == EffectType::None) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    switch (m_effectType) {
    case EffectType::Bomb:
        drawBombEffect(p);
        break;
    case EffectType::Rocket:
        drawRocketEffect(p);
        break;
    case EffectType::Special523:
        drawSpecialEffect(p);
        break;
    default:
        break;
    }
}

void PlayEffectOverlay::drawBombEffect(QPainter& p)
{
    int cx = width() / 2;
    int cy = height() / 2;
    int maxR = qMin(width(), height()) * 0.6;
    float t = m_effectProgress;

    // 背景闪光（快速衰减）
    float flashAlpha = qMax(0.0f, 0.5f * (1.0f - t * 2.0f));
    if (flashAlpha > 0) {
        p.fillRect(rect(), QColor(255, 180, 60, static_cast<int>(flashAlpha * 255)));
    }

    // 冲击波（从中心向外扩散）
    QColor bombColor(255, 100, 20);
    drawShockwave(p, bombColor, cx, cy, t, maxR);

    // 第二波冲击波（延迟一点）
    float t2 = qMax(0.0f, t - 0.2f) / 0.8f;
    QColor bombColor2(255, 200, 60);
    drawShockwave(p, bombColor2, cx, cy, t2, maxR * 0.85);

    // 炸弹符号（💥），先放大后缩小
    float symbolScale = 0.0f;
    if (t < 0.3f) {
        symbolScale = t / 0.3f;  // 弹出
    } else {
        symbolScale = 1.0f + 0.3f * (1.0f - (t - 0.3f) / 0.7f);  // 轻微收缩
    }
    int symbolSize = static_cast<int>(qMin(width(), height()) * 0.35 * symbolScale);
    if (symbolSize > 0) {
        QFont f;
        f.setPixelSize(symbolSize);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 240));
        p.drawText(QRect(cx - symbolSize, cy - symbolSize,
                         symbolSize * 2, symbolSize * 2),
                   Qt::AlignCenter, "💥");
    }

    // 文字"炸弹！"
    if (t > 0.4f) {
        float textAlpha = qMin(1.0f, (t - 0.4f) / 0.2f);
        QFont textFont;
        textFont.setPixelSize(36);
        textFont.setBold(true);
        p.setFont(textFont);
        p.setPen(QColor(255, 200, 50, static_cast<int>(textAlpha * 255)));
        // 文字阴影
        p.drawText(QRect(cx - 200, cy + maxR * 0.5f, 400, 50),
                   Qt::AlignCenter, "炸  弹！");
    }
}

void PlayEffectOverlay::drawRocketEffect(QPainter& p)
{
    int cx = width() / 2;
    int cy = height() / 2;
    int maxR = qMin(width(), height()) * 0.7;
    float t = m_effectProgress;

    // 金色背景光晕（全屏闪烁）
    float flashAlpha = qMax(0.0f, 0.6f * (1.0f - t * 1.5f));
    if (flashAlpha > 0) {
        QRadialGradient flashGrad(cx, cy, maxR * 1.5);
        flashGrad.setColorAt(0.0, QColor(255, 215, 0, static_cast<int>(flashAlpha * 255)));
        flashGrad.setColorAt(0.5, QColor(255, 180, 0, static_cast<int>(flashAlpha * 180)));
        flashGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), flashGrad);
    }

    // 多道冲击波（金色）
    QColor gold(255, 200, 0);
    drawShockwave(p, gold, cx, cy, t, maxR);

    float t2 = qMax(0.0f, t - 0.15f) / 0.85f;
    QColor gold2(255, 230, 100);
    drawShockwave(p, gold2, cx, cy, t2, maxR * 0.9);

    float t3 = qMax(0.0f, t - 0.3f) / 0.7f;
    QColor purple(180, 80, 255);
    drawShockwave(p, purple, cx, cy, t3, maxR * 0.75);

    // 王炸符号（👑），弹跳效果
    float bounceT = qMin(t / 0.5f, 1.0f);
    float bounce = 1.0f - qAbs(qSin(bounceT * M_PI * 2)) * 0.2f;
    int symbolSize = static_cast<int>(qMin(width(), height()) * 0.30 * bounce);
    if (symbolSize > 0) {
        QFont f;
        f.setPixelSize(symbolSize);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 250));
        p.drawText(QRect(cx - symbolSize, cy - symbolSize,
                         symbolSize * 2, symbolSize * 2),
                   Qt::AlignCenter, "👑");
    }

    // 文字"王炸！"
    if (t > 0.3f) {
        float textAlpha = qMin(1.0f, (t - 0.3f) / 0.2f);
        QFont textFont;
        textFont.setPixelSize(42);
        textFont.setBold(true);
        p.setFont(textFont);

        // 描边效果
        p.setPen(QPen(QColor(120, 40, 180, static_cast<int>(textAlpha * 255)), 4));
        p.drawText(QRect(cx - 200, cy + maxR * 0.45f, 400, 56),
                   Qt::AlignCenter, "王  炸！");
        p.setPen(QColor(255, 215, 0, static_cast<int>(textAlpha * 255)));
        p.drawText(QRect(cx - 200, cy + maxR * 0.45f, 400, 56),
                   Qt::AlignCenter, "王  炸！");
    }
}

void PlayEffectOverlay::drawSpecialEffect(QPainter& p)
{
    int cx = width() / 2;
    int cy = height() / 2;
    int maxR = qMin(width(), height()) * 0.8;
    float t = m_effectProgress;

    // 七彩背景闪烁
    float flashAlpha = qMax(0.0f, 0.7f * (1.0f - t * 1.2f));
    if (flashAlpha > 0) {
        // 彩虹渐变
        QConicalGradient rainbow(cx, cy, 0);
        rainbow.setColorAt(0.0, QColor(255, 0, 0, static_cast<int>(flashAlpha * 200)));
        rainbow.setColorAt(0.17, QColor(255, 165, 0, static_cast<int>(flashAlpha * 200)));
        rainbow.setColorAt(0.33, QColor(255, 255, 0, static_cast<int>(flashAlpha * 200)));
        rainbow.setColorAt(0.5, QColor(0, 255, 0, static_cast<int>(flashAlpha * 200)));
        rainbow.setColorAt(0.67, QColor(0, 150, 255, static_cast<int>(flashAlpha * 200)));
        rainbow.setColorAt(0.83, QColor(75, 0, 130, static_cast<int>(flashAlpha * 200)));
        rainbow.setColorAt(1.0, QColor(255, 0, 0, static_cast<int>(flashAlpha * 200)));
        p.fillRect(rect(), rainbow);
    }

    // 旋转光芒（射线效果）
    if (t < 0.8f) {
        float rayAlpha = qMin(1.0f, t / 0.2f) * (1.0f - (t - 0.2f) / 0.6f);
        p.save();
        p.translate(cx, cy);
        p.rotate(t * 360);  // 旋转

        int rayCount = 12;
        for (int i = 0; i < rayCount; ++i) {
            p.rotate(360.0 / rayCount);
            QLinearGradient rayGrad(0, 0, maxR, 0);
            rayGrad.setColorAt(0.0, QColor(255, 255, 255, static_cast<int>(rayAlpha * 200)));
            rayGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setBrush(rayGrad);
            p.setPen(Qt::NoPen);
            // 绘制三角光芒
            QPolygon ray;
            ray << QPoint(0, -3)
                << QPoint(maxR, -8)
                << QPoint(maxR, 8)
                << QPoint(0, 3);
            p.drawPolygon(ray);
        }
        p.restore();
    }

    // Special523 符号（✨🎴）
    float symbolScale = 0.0f;
    if (t < 0.3f) {
        symbolScale = t / 0.3f;
    } else {
        symbolScale = 1.0f + 0.15f * qSin((t - 0.3f) * M_PI * 4);
    }
    int symbolSize = static_cast<int>(qMin(width(), height()) * 0.25 * symbolScale);
    if (symbolSize > 0) {
        QFont f;
        f.setPixelSize(symbolSize);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 240));
        p.drawText(QRect(cx - symbolSize * 2, cy - symbolSize,
                         symbolSize * 4, symbolSize * 2),
                   Qt::AlignCenter, "✨ 523 ✨");
    }

    // 文字
    if (t > 0.25f) {
        float textAlpha = qMin(1.0f, (t - 0.25f) / 0.15f);
        QFont textFont;
        textFont.setPixelSize(48);
        textFont.setBold(true);
        p.setFont(textFont);

        // 彩虹文字效果（用渐变笔刷模拟）
        QLinearGradient textGrad(cx - 200, 0, cx + 200, 0);
        textGrad.setColorAt(0.0, QColor(255, 80, 80));
        textGrad.setColorAt(0.33, QColor(255, 220, 50));
        textGrad.setColorAt(0.66, QColor(80, 255, 120));
        textGrad.setColorAt(1.0, QColor(100, 150, 255));
        p.setPen(QPen(QColor(0, 0, 0, static_cast<int>(textAlpha * 200)), 3));
        p.drawText(QRect(cx - 250, cy + maxR * 0.4f, 500, 64),
                   Qt::AlignCenter, "七鬼523！");
        p.setPen(QColor(255, 215, 0, static_cast<int>(textAlpha * 255)));
        p.drawText(QRect(cx - 250, cy + maxR * 0.4f, 500, 64),
                   Qt::AlignCenter, "七鬼523！");
    }
}

void PlayEffectOverlay::drawShockwave(QPainter& p, const QColor& color,
                                       int cx, int cy, float progress,
                                       int maxRadius)
{
    if (progress <= 0 || progress >= 1.0f) return;

    int radius = static_cast<int>(maxRadius * progress);
    int alpha = static_cast<int>(255 * (1.0f - progress));
    int lineWidth = qMax(2, static_cast<int>(20 * (1.0f - progress)));

    QPen pen(QColor(color.red(), color.green(), color.blue(), alpha));
    pen.setWidth(lineWidth);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
}
