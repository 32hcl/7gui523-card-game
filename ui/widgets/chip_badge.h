#ifndef CHIP_BADGE_H
#define CHIP_BADGE_H

#include <QWidget>
#include <QLabel>
#include "ui/widgets/chip_widget.h"

/**
 * @brief 筹码余额徽章
 *
 * 显示当前筹码余额的徽章组件，包含：
 * - 小筹码图标
 * - 筹码数值（带千分位）
 * - "筹码"标签文字
 *
 * 风格：国潮赌场风（深红金渐变背景 + 金边）
 */
class ChipBadge : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int chips READ chips WRITE setChips NOTIFY chipsChanged)

public:
    explicit ChipBadge(QWidget* parent = nullptr);
    explicit ChipBadge(int chips, QWidget* parent = nullptr);

    int chips() const { return m_chips; }

public slots:
    void setChips(int chips);
    void addChips(int amount);
    bool spendChips(int amount);

signals:
    void chipsChanged(int newChips);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUi();
    void updateDisplay();
    static QString formatNumber(int value);

    int       m_chips;
    QLabel*   m_valueLabel;
    QLabel*   m_label;
    ChipWidget* m_chipIcon;
};

#endif // CHIP_BADGE_H
