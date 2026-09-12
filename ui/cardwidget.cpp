#include "cardwidget.h"
#include <QPainter>
#include <QPen>
#include <QFontMetrics>

// ── 构造 ──────────────────────────────────────────────────────────
CardWidget::CardWidget(const Card& card, QWidget* parent)
    : QWidget(parent), m_card(card)
{
    setFixedSize(80, 140);
}

// ── 公开接口 ──────────────────────────────────────────────────────
Card CardWidget::getCard() const
{
    return m_card;
}

void CardWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

bool CardWidget::isSelected() const
{
    return m_selected;
}

// ── 鼠标事件 ──────────────────────────────────────────────────────
void CardWidget::mousePressEvent(QMouseEvent* /*event*/)
{
    m_selected = !m_selected;
    update();
    emit clicked();
}

// ── 私有辅助 ──────────────────────────────────────────────────────
QColor CardWidget::textColor() const
{
    if (m_card.point == "大鬼")
        return Qt::red;
    if (m_card.point == "小鬼")
        return Qt::black;
    if (m_card.suit == "红桃" || m_card.suit == "方块")
        return Qt::red;
    return Qt::black;
}

QString CardWidget::suitSymbol() const
{
    if (m_card.point == "大鬼") return QStringLiteral("\u2605");   // ★
    if (m_card.point == "小鬼") return QStringLiteral("\u2606");   // ☆
    if (m_card.suit == "黑桃")  return QStringLiteral("\u2660");   // ♠
    if (m_card.suit == "红桃")  return QStringLiteral("\u2665");   // ♥
    if (m_card.suit == "梅花")  return QStringLiteral("\u2663");   // ♣
    if (m_card.suit == "方块")  return QStringLiteral("\u2666");   // ♦
    return "";
}

int CardWidget::pointFontSize() const
{
    // 大鬼 / 小鬼 两个字，缩小一点防止溢出
    if (m_card.point == "大鬼" || m_card.point == "小鬼")
        return 13;
    return 16;
}

// ── 自绘 ──────────────────────────────────────────────────────────
void CardWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int yOffset = m_selected ? 1 : 21;   // 选中时向上移动 20px

    // ── 背景圆角矩形 ──────────────────────────────────────────
    if (m_selected) {
        p.setPen(QPen(QColor(255, 200, 0), 3));    // 金色边框
    } else {
        p.setPen(QPen(Qt::black, 2));
    }
    p.setBrush(Qt::white);
    p.drawRoundedRect(1, yOffset, w - 2, 118, 8, 8);

    const QColor color = textColor();
    p.setPen(color);

    // ── 左上角：点数 ──────────────────────────────────────────
    QFont ptFont = font();
    ptFont.setPixelSize(pointFontSize());
    ptFont.setBold(true);
    p.setFont(ptFont);

    const QString ptText = QString::fromStdString(m_card.point);
    p.drawText(6, yOffset + 4, w - 12, 22, Qt::AlignLeft | Qt::AlignTop, ptText);

    // ── 中央：大花色标记（增加视觉效果） ──────────────────────
    if (m_card.point != "大鬼" && m_card.point != "小鬼") {
        QFont centerFont = font();
        centerFont.setPixelSize(32);
        p.setFont(centerFont);
        p.setOpacity(0.20);
        p.drawText(0, yOffset, w, 118, Qt::AlignCenter, suitSymbol());
        p.setOpacity(1.0);
    }

    // ── 右下角：花色 / 鬼标记 ──────────────────────────────────
    QFont suitFont = font();
    suitFont.setPixelSize(14);
    suitFont.setBold(true);
    p.setFont(suitFont);
    p.setPen(color);

    p.drawText(w - 34, yOffset + 118 - 28, 30, 24,
               Qt::AlignRight | Qt::AlignBottom, suitSymbol());

    // ── 底部：分值 ────────────────────────────────────────────
    if (m_card.score > 0) {
        QFont scFont = font();
        scFont.setPixelSize(10);
        p.setFont(scFont);
        p.setPen(QColor(120, 120, 120));

        const QString scText = QString::number(m_card.score) + QStringLiteral("\u5206"); // 分
        p.drawText(0, yOffset + 118 - 18, w, 16, Qt::AlignCenter, scText);
    }
}