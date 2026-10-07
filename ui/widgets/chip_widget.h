#ifndef CHIP_WIDGET_H
#define CHIP_WIDGET_H

#include <QWidget>
#include <QVector>

/**
 * @brief 筹码堆叠显示组件
 *
 * 根据数值自动计算四色筹码的堆叠方式：
 * - 黑筹码: 50
 * - 绿筹码: 10
 * - 蓝筹码: 5
 * - 红筹码: 1
 *
 * 可设置显示方向（水平/垂直堆叠）和尺寸。
 */
class ChipWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

public:
    enum class Orientation {
        Horizontal,  // 水平排列（从左到右：红→蓝→绿→黑）
        Stacked      // 堆叠显示（从下到上：黑→绿→蓝→红）
    };

    explicit ChipWidget(QWidget* parent = nullptr);
    explicit ChipWidget(int value, QWidget* parent = nullptr);

    int value() const { return m_value; }
    void setValue(int value);

    Orientation orientation() const { return m_orientation; }
    void setOrientation(Orientation o);

    /**
     * @brief 设置筹码尺寸（直径）
     */
    void setChipSize(int size);
    int chipSize() const { return m_chipSize; }

    /**
     * @brief 显示数值标签
     */
    void setShowLabel(bool show);
    bool showLabel() const { return m_showLabel; }

    QSize sizeHint() const override;

signals:
    void valueChanged(int newValue);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct ChipCount {
        int black = 0;   // 50
        int green = 0;   // 10
        int blue  = 0;   // 5
        int red   = 0;   // 1
    };

    void recalcChips();
    void drawChip(QPainter& p, int x, int y, const QColor& color, const QString& label);
    QSize computeSize() const;

    int          m_value;
    Orientation  m_orientation;
    int          m_chipSize;   // 筹码直径
    bool         m_showLabel;  // 是否显示数值

    ChipCount    m_chipCount;
};

#endif // CHIP_WIDGET_H
