#ifndef PLAY_EFFECT_OVERLAY_H
#define PLAY_EFFECT_OVERLAY_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QPropertyAnimation>

/**
 * @brief 出牌特效遮罩层
 *
 * 覆盖在桌面上的半透明特效层，支持：
 * - 炸弹特效（💥 + 橙红光晕 + 震屏）
 * - 王炸特效（👑 + 金紫光晕 + 放大动画）
 * - Special523特效（🌈 + 七彩光芒 + 无敌动画）
 *
 * 使用方式：
 *   1. 将此widget作为桌面区域的child，设为stacked最上层
 *   2. 调用 showBombEffect() / showRocketEffect() / showSpecialEffect()
 *   3. 动画播放完后自动隐藏
 */
class PlayEffectOverlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float effectProgress READ effectProgress WRITE setEffectProgress)

public:
    enum class EffectType {
        None,
        Bomb,        // 炸弹
        Rocket,      // 王炸
        Special523,  // 七鬼523
    };

    explicit PlayEffectOverlay(QWidget* parent = nullptr);

    float effectProgress() const { return m_effectProgress; }
    void setEffectProgress(float p);

    /**
     * @brief 显示炸弹特效
     * @param durationMs 持续时间（毫秒）
     */
    void showBombEffect(int durationMs = 800);

    /**
     * @brief 显示王炸特效
     */
    void showRocketEffect(int durationMs = 1000);

    /**
     * @brief 显示Special523无敌特效
     */
    void showSpecial523Effect(int durationMs = 1500);

signals:
    void effectFinished();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimationFinished();

private:
    void startEffect(EffectType type, int durationMs);
    void drawBombEffect(QPainter& p);
    void drawRocketEffect(QPainter& p);
    void drawSpecialEffect(QPainter& p);
    void drawShockwave(QPainter& p, const QColor& color, int cx, int cy,
                       float progress, int maxRadius);

    EffectType          m_effectType;
    float               m_effectProgress;  // 0.0 ~ 1.0
    QPropertyAnimation* m_anim;
    QTimer*             m_hideTimer;
};

#endif // PLAY_EFFECT_OVERLAY_H
