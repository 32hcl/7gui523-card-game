#include "ui/widgets/boss_avatar.h"
#include "ai/ai_levels.h"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QString>

// ── Boss 样式定义 ──
// 每个Boss独特的配色和符号
static const BossAvatar::BossStyle g_bossStyles[] = {
    // Level 1: 傻子/新手 - 嫩绿（简单、无害）
    {
        QColor("#84cc16"), QColor("#4d7c0f"), QColor("#a3e635"),
        "😐", "新手"
    },
    // Level 2: 换牌虫/骗子 - 紫色（神秘、狡诈）
    {
        QColor("#7c3aed"), QColor("#4c1d95"), QColor("#a78bfa"),
        "🎭", "换牌虫"
    },
    // Level 3: 急眼雀/急性子 - 橙色（急躁、火爆）
    {
        QColor("#f97316"), QColor("#9a3412"), QColor("#fdba74"),
        "⚡", "急眼雀"
    },
    // Level 4: 疯狗 - 血红（狂暴、危险）
    {
        QColor("#dc2626"), QColor("#7f1d1d"), QColor("#fca5a5"),
        "🐕", "疯狗"
    },
    // Level 5: 赖账鬼 - 深褐（阴暗、耍赖）
    {
        QColor("#78350f"), QColor("#451a03"), QColor("#d97706"),
        "👻", "赖账鬼"
    },
    // Level 6: 不识数 - 灰蓝（糊涂、愚笨）
    {
        QColor("#475569"), QColor("#1e293b"), QColor("#94a3b8"),
        "🤪", "不识数"
    },
    // Level 7: 黑老大 - 靛青
    {
        QColor("#0891b2"), QColor("#164e63"), QColor("#67e8f9"),
        "🔱", "黑老大"
    },
    // Level 8: 千里眼 - 粉紫
    {
        QColor("#db2777"), QColor("#831843"), QColor("#f9a8d4"),
        "👁️", "千里眼"
    },
    // Level 9: 赌场老板 - 金黑（最终BOSS）
    {
        QColor("#1f2937"), QColor("#030712"), QColor("#ffd700"),
        "🎩", "赌场老板"
    },
};

static const int BOSS_COUNT = 9;

const BossAvatar::BossStyle& BossAvatar::getStyle(int level)
{
    static const BossStyle defaultStyle = {
        QColor("#6b7280"), QColor("#374151"), QColor("#9ca3af"),
        "❓", "未知"
    };
    if (level < 1 || level > BOSS_COUNT) return defaultStyle;
    return g_bossStyles[level - 1];
}

BossAvatar::BossAvatar(QWidget* parent)
    : QWidget(parent)
    , m_level(1)
    , m_size(80)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

BossAvatar::BossAvatar(int level, QWidget* parent)
    : BossAvatar(parent)
{
    setLevel(level);
}

void BossAvatar::setLevel(int level)
{
    level = qMax(1, qMin(BOSS_COUNT, level));
    if (m_level == level) return;
    m_level = level;
    update();
    emit levelChanged(m_level);
}

void BossAvatar::setAvatarSize(int size)
{
    size = qMax(32, size);
    if (m_size == size) return;
    m_size = size;
    setFixedSize(m_size, m_size);
    update();
}

QString BossAvatar::bossName() const
{
    return QString::fromStdString(getBossName(m_level));
}

QSize BossAvatar::sizeHint() const
{
    return QSize(m_size, m_size);
}

void BossAvatar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    int cx = width() / 2;
    int cy = height() / 2;
    int radius = qMin(width(), height()) / 2 - 2;

    drawAvatar(p, cx, cy, radius);
}

void BossAvatar::drawAvatar(QPainter& p, int cx, int cy, int radius)
{
    const BossStyle& style = getStyle(m_level);

    // 外层光晕
    QRadialGradient outerGlow(cx, cy, radius * 1.2, cx, cy, radius);
    outerGlow.setColorAt(0.0, QColor(style.accent.red(), style.accent.green(), style.accent.blue(), 80));
    outerGlow.setColorAt(0.7, QColor(style.accent.red(), style.accent.green(), style.accent.blue(), 20));
    outerGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(outerGlow);
    p.drawEllipse(cx - radius * 1.2, cy - radius * 1.2,
                  radius * 2.4, radius * 2.4);

    // 主体圆形（径向渐变）
    QRadialGradient bodyGrad(cx - radius/4, cy - radius/4, radius,
                             cx - radius/3, cy - radius/3, radius * 0.3);
    bodyGrad.setColorAt(0.0, style.primary.lighter(130));
    bodyGrad.setColorAt(0.6, style.primary);
    bodyGrad.setColorAt(1.0, style.secondary);
    p.setBrush(bodyGrad);
    p.setPen(QPen(style.accent, 3));
    p.drawEllipse(cx - radius, cy - radius, 2 * radius, 2 * radius);

    // 内圈装饰线
    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(cx - radius + 6, cy - radius + 6, 2 * radius - 12, 2 * radius - 12);

    // 符号
    drawSymbol(p, cx, cy, radius, style);

    // 顶部金色装饰（皇冠/头饰感）
    int decorY = cy - radius + 4;
    p.setPen(Qt::NoPen);
    p.setBrush(style.accent);
    // 小圆点装饰
    for (int i = -1; i <= 1; ++i) {
        int dx = cx + i * (radius / 3);
        p.drawEllipse(dx - 2, decorY - 2, 4, 4);
    }
}

void BossAvatar::drawSymbol(QPainter& p, int cx, int cy, int radius, const BossStyle& style)
{
    QFont symbolFont;
    symbolFont.setPixelSize(radius * 0.8);
    p.setFont(symbolFont);
    p.setPen(QColor(255, 255, 255, 240));
    p.drawText(QRect(cx - radius, cy - radius, 2 * radius, 2 * radius),
               Qt::AlignCenter, style.symbol);
}
