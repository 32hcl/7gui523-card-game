#include "ui/widgets/chip_badge.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QFont>

ChipBadge::ChipBadge(QWidget* parent)
    : QWidget(parent)
    , m_chips(0)
    , m_valueLabel(nullptr)
    , m_label(nullptr)
    , m_chipIcon(nullptr)
{
    setupUi();
}

ChipBadge::ChipBadge(int chips, QWidget* parent)
    : ChipBadge(parent)
{
    setChips(chips);
}

void ChipBadge::setupUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(R"(
        ChipBadge {
            background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(42,26,20,0.95),
                stop:1 rgba(26,15,10,0.9));
            border: 1px solid rgba(255,215,0,0.4);
            border-radius: 22px;
        }
    )");

    auto* mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(12, 8, 16, 8);
    mainLay->setSpacing(10);

    // 左侧筹码图标
    m_chipIcon = new ChipWidget(0, this);
    m_chipIcon->setChipSize(28);
    m_chipIcon->setFixedSize(32, 32);
    mainLay->addWidget(m_chipIcon);

    // 右侧文字区
    auto* textLay = new QVBoxLayout();
    textLay->setContentsMargins(0, 0, 0, 0);
    textLay->setSpacing(0);

    m_valueLabel = new QLabel("0");
    QFont valFont = m_valueLabel->font();
    valFont.setPointSize(14);
    valFont.setBold(true);
    valFont.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    m_valueLabel->setFont(valFont);
    m_valueLabel->setStyleSheet("QLabel { color: #ffe66d; background: transparent; }");
    textLay->addWidget(m_valueLabel);

    m_label = new QLabel("筹码");
    QFont lblFont = m_label->font();
    lblFont.setPointSize(10);
    m_label->setFont(lblFont);
    m_label->setStyleSheet("QLabel { color: #c9b896; background: transparent; }");
    textLay->addWidget(m_label);

    mainLay->addLayout(textLay);

    updateDisplay();
}

void ChipBadge::setChips(int chips)
{
    chips = qMax(0, chips);
    if (m_chips == chips) return;
    m_chips = chips;
    updateDisplay();
    emit chipsChanged(m_chips);
}

void ChipBadge::addChips(int amount)
{
    if (amount <= 0) return;
    setChips(m_chips + amount);
}

bool ChipBadge::spendChips(int amount)
{
    if (amount <= 0 || m_chips < amount) return false;
    setChips(m_chips - amount);
    return true;
}

void ChipBadge::updateDisplay()
{
    m_valueLabel->setText(formatNumber(m_chips));
    m_chipIcon->setValue(m_chips);
}

QString ChipBadge::formatNumber(int value)
{
    QString numStr = QString::number(value);
    // 添加千分位逗号
    for (int i = numStr.length() - 3; i > 0; i -= 3) {
        numStr.insert(i, ',');
    }
    return numStr;
}

void ChipBadge::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}
