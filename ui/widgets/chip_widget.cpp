#include "ui/widgets/chip_widget.h"

#include <QPainter>
#include <QPen>
#include <QFont>
#include <QFontMetrics>
#include <QRadialGradient>
#include <QLinearGradient>

// 筹码面额定义
static const int CHIP_BLACK_VALUE = 50;
static const int CHIP_GREEN_VALUE = 10;
static const int CHIP_BLUE_VALUE  = 5;
static const int CHIP_RED_VALUE   = 1;

// 筹码颜色
static const QColor CHIP_BLACK_COLOR("#1f2937");
static const QColor CHIP_GREEN_COLOR("#15803d");
static const QColor CHIP_BLUE_COLOR("#1d4ed8");
static const QColor CHIP_RED_COLOR("#dc2626");

// 筹码金边颜色
static const QColor CHIP_GOLD_LIGHT("#ffe66d");
static const QColor CHIP_GOLD("#ffd700");
static const QColor CHIP_GOLD_DARK("#b8860b");

ChipWidget::ChipWidget(QWidget* parent)
    : QWidget(parent)
    , m_value(0)
    , m_orientation(Orientation::Horizontal)
    , m_chipSize(32)
    , m_showLabel(false)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

ChipWidget::ChipWidget(int value, QWidget* parent)
    : ChipWidget(parent)
{
    setValue(value);
}

void ChipWidget::setValue(int value)
{
    value = qMax(0, value);
    if (m_value == value) return;
    m_value = value;
    recalcChips();
    updateGeometry();
    update();
    emit valueChanged(m_value);
}

void ChipWidget::setOrientation(Orientation o)
{
    if (m_orientation == o) return;
    m_orientation = o;
    updateGeometry();
    update();
}

void ChipWidget::setChipSize(int size)
{
    if (m_chipSize == size) return;
    m_chipSize = qMax(16, size);
    updateGeometry();
    update();
}

void ChipWidget::setShowLabel(bool show)
{
    if (m_showLabel == show) return;
    m_showLabel = show;
    updateGeometry();
    update();
}

void ChipWidget::recalcChips()
{
    int remain = m_value;
    m_chipCount.black = remain / CHIP_BLACK_VALUE;
    remain -= m_chipCount.black * CHIP_BLACK_VALUE;
    m_chipCount.green = remain / CHIP_GREEN_VALUE;
    remain -= m_chipCount.green * CHIP_GREEN_VALUE;
    m_chipCount.blue = remain / CHIP_BLUE_VALUE;
    remain -= m_chipCount.blue * CHIP_BLUE_VALUE;
    m_chipCount.red = remain;
}

QSize ChipWidget::computeSize() const
{
    int w = 0, h = 0;
    int chip = m_chipSize;
    const int spacing = chip / 4;

    if (m_orientation == Orientation::Horizontal) {
        int count = 0;
        if (m_chipCount.black > 0) count++;
        if (m_chipCount.green > 0) count++;
        if (m_chipCount.blue > 0)  count++;
        if (m_chipCount.red > 0)   count++;
        if (count == 0) count = 1; // 至少显示一个

        w = count * chip + (count - 1) * spacing;
        h = chip;
    } else {
        // Stacked: 垂直堆叠，最多显示每色3个（更多则标注数字）
        int maxStack = 0;
        maxStack = qMax(maxStack, qMin(m_chipCount.black, 3));
        maxStack = qMax(maxStack, qMin(m_chipCount.green, 3));
        maxStack = qMax(maxStack, qMin(m_chipCount.blue, 3));
        maxStack = qMax(maxStack, qMin(m_chipCount.red, 3));
        if (maxStack == 0) maxStack = 1;

        w = 4 * chip + 3 * spacing; // 四色并排
        int stackH = maxStack * (chip * 3 / 4) + chip / 4;
        h = stackH;
    }

    if (m_showLabel) {
        QFont font;
        font.setBold(true);
        font.setPixelSize(chip / 2);
        QFontMetrics fm(font);
        int textW = fm.horizontalAdvance(QString::number(m_value)) + 20;
        w = qMax(w, textW);
        h += chip / 2 + 4;
    }

    return QSize(w + 4, h + 4);
}

QSize ChipWidget::sizeHint() const
{
    return computeSize();
}

void ChipWidget::drawChip(QPainter& p, int x, int y, const QColor& color, const QString& label)
{
    int size = m_chipSize;
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2 - 1;

    // 阴影
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawEllipse(cx - r + 1, cy - r + 2, 2 * r, 2 * r);

    // 主体（径向渐变：边缘暗，中间亮）
    QRadialGradient grad(cx, cy, r, cx - r/3, cy - r/3);
    grad.setColorAt(0.0, color.lighter(130));
    grad.setColorAt(0.7, color);
    grad.setColorAt(1.0, color.darker(150));
    p.setBrush(grad);
    p.setPen(QPen(CHIP_GOLD_DARK, 1.5));
    p.drawEllipse(cx - r, cy - r, 2 * r, 2 * r);

    // 金色内圈
    p.setPen(QPen(CHIP_GOLD, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(cx - r + 4, cy - r + 4, 2 * r - 8, 2 * r - 8);

    // 内圈填充
    QRadialGradient innerGrad(cx, cy, r - 6, cx - r/4, cy - r/4);
    innerGrad.setColorAt(0.0, color.lighter(115));
    innerGrad.setColorAt(1.0, color.darker(120));
    p.setBrush(innerGrad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(cx - r + 5, cy - r + 5, 2 * r - 10, 2 * r - 10);

    // 面额文字
    if (!label.isEmpty()) {
        QFont font;
        font.setBold(true);
        font.setPixelSize(size / 3);
        p.setFont(font);
        p.setPen(CHIP_GOLD_LIGHT);
        p.drawText(QRect(cx - r, cy - r, 2 * r, 2 * r),
                   Qt::AlignCenter, label);
    }
}

void ChipWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int chip = m_chipSize;
    const int spacing = chip / 4;

    if (m_orientation == Orientation::Horizontal) {
        int x = 2;
        int y = 2;

        // 从大到小排列：黑→绿→蓝→红
        if (m_chipCount.black > 0) {
            drawChip(p, x, y, CHIP_BLACK_COLOR, "50");
            x += chip + spacing;
        }
        if (m_chipCount.green > 0) {
            drawChip(p, x, y, CHIP_GREEN_COLOR, "10");
            x += chip + spacing;
        }
        if (m_chipCount.blue > 0) {
            drawChip(p, x, y, CHIP_BLUE_COLOR, "5");
            x += chip + spacing;
        }
        if (m_chipCount.red > 0) {
            drawChip(p, x, y, CHIP_RED_COLOR, "1");
            x += chip + spacing;
        }

        // 如果值为0，显示一个灰色空筹码
        if (m_value == 0) {
            drawChip(p, x, y, QColor("#6b7280"), "0");
        }
    } else {
        // Stacked mode: 四色并排，每色堆叠
        int startY = height() - chip - 2;
        int x = 2;
        const int stackOffset = chip / 4;

        auto drawStack = [&](int count, const QColor& color, const QString& lbl) {
            if (count <= 0) return;
            int displayCount = qMin(count, 3);
            for (int i = 0; i < displayCount; ++i) {
                drawChip(p, x, startY - i * stackOffset, color,
                         (i == 0 && count > 3) ? QString::number(count) : "");
            }
            x += chip + spacing;
        };

        drawStack(m_chipCount.black, CHIP_BLACK_COLOR, "50");
        drawStack(m_chipCount.green, CHIP_GREEN_COLOR, "10");
        drawStack(m_chipCount.blue,  CHIP_BLUE_COLOR,  "5");
        drawStack(m_chipCount.red,   CHIP_RED_COLOR,   "1");

        if (m_value == 0) {
            drawChip(p, x, startY, QColor("#6b7280"), "0");
        }
    }

    // 数值标签
    if (m_showLabel) {
        QFont font;
        font.setBold(true);
        font.setPixelSize(chip / 2);
        p.setFont(font);
        p.setPen(CHIP_GOLD);
        p.drawText(QRect(0, height() - chip/2 - 2, width(), chip/2),
                   Qt::AlignCenter, QString::number(m_value));
    }
}
