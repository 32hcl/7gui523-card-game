#include "cardwidget.h"
#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QCoreApplication>
#include <QFile>

CardWidget::CardWidget(const Card& card, QWidget* parent)
    : QWidget(parent), m_card(card), m_currentYOffset(21)
{
    setFixedSize(110, 165);
    loadPixmap();
}

Card CardWidget::getCard() const
{
    return m_card;
}

void CardWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        QPropertyAnimation* anim = new QPropertyAnimation(this, "currentYOffset");
        anim->setDuration(150);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(m_currentYOffset);
        anim->setEndValue(m_selected ? 1 : 21);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

bool CardWidget::isSelected() const
{
    return m_selected;
}

void CardWidget::setCurrentYOffset(int v)
{
    m_currentYOffset = v;
    update();
}

void CardWidget::mousePressEvent(QMouseEvent*)
{
    m_selected = !m_selected;

    QPropertyAnimation* anim = new QPropertyAnimation(this, "currentYOffset");
    anim->setDuration(150);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(m_currentYOffset);
    anim->setEndValue(m_selected ? 1 : 21);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    emit clicked();
}

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
    if (m_card.point == "大鬼") return QStringLiteral("\u2605");
    if (m_card.point == "小鬼") return QStringLiteral("\u2606");
    if (m_card.suit == "黑桃")  return QStringLiteral("\u2660");
    if (m_card.suit == "红桃")  return QStringLiteral("\u2665");
    if (m_card.suit == "梅花")  return QStringLiteral("\u2663");
    if (m_card.suit == "方块")  return QStringLiteral("\u2666");
    return "";
}

int CardWidget::pointFontSize() const
{
    if (m_card.point == "大鬼" || m_card.point == "小鬼")
        return 13;
    return 16;
}

QString CardWidget::cardImageFileName(const Card& card) {
    if (card.point == "大鬼") {
        return "card_joker_red.png";
    } else if (card.point == "小鬼") {
        return "card_joker_black.png";
    }

    QString suitKey;
    if (card.suit == "黑桃") suitKey = "spades";
    else if (card.suit == "红桃") suitKey = "hearts";
    else if (card.suit == "梅花") suitKey = "clubs";
    else if (card.suit == "方块") suitKey = "diamonds";
    else return QString();

    QString pointKey = QString::fromStdString(card.point);
    if (pointKey != "A" && pointKey != "J" && pointKey != "Q" && pointKey != "K") {
        bool ok = false;
        int num = pointKey.toInt(&ok);
        if (ok) {
            pointKey = QString("%1").arg(num, 2, 10, QChar('0'));
        }
    }

    return QString("card_%1_%2.png").arg(suitKey).arg(pointKey);
}

void CardWidget::loadPixmap() {
    QString cardsDir = QCoreApplication::applicationDirPath() + "/cards/";
    QString fileName = cardImageFileName(m_card);
    QString fullPath = cardsDir + fileName;
    if (QFile::exists(fullPath)) {
        m_pixmap = QPixmap(fullPath);
    }
}

void CardWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();      // 100
    const int h = height();     // 165
    const int yOffset = m_currentYOffset;  // 动画值

    if (!m_pixmap.isNull()) {
        QPixmap scaled = m_pixmap.scaled(w - 4, 143,
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        int x = (w - scaled.width()) / 2;
        p.drawPixmap(x, yOffset, scaled);

        if (m_selected && !m_flying) {
            p.setPen(QPen(QColor(255, 200, 0), 3));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(1, yOffset, w - 2, 143, 8, 8);
        }
    } else {
        if (m_selected && !m_flying) {
            p.setPen(QPen(QColor(255, 200, 0), 3));
        } else {
            p.setPen(QPen(Qt::black, 2));
        }
        p.setBrush(Qt::white);
        p.drawRoundedRect(1, yOffset, w - 2, 143, 8, 8);

        const QColor color = textColor();
        p.setPen(color);

        QFont ptFont = font();
        ptFont.setPixelSize(pointFontSize());
        ptFont.setBold(true);
        p.setFont(ptFont);

        const QString ptText = QString::fromStdString(m_card.point);
        p.drawText(6, yOffset + 4, w - 12, 22, Qt::AlignLeft | Qt::AlignTop, ptText);

        if (m_card.point != "大鬼" && m_card.point != "小鬼") {
            QFont centerFont = font();
            centerFont.setPixelSize(34);
            p.setFont(centerFont);
            p.setOpacity(0.20);
            p.drawText(0, yOffset, w, 143, Qt::AlignCenter, suitSymbol());
            p.setOpacity(1.0);
        }

        QFont suitFont = font();
        suitFont.setPixelSize(16);
        suitFont.setBold(true);
        p.setFont(suitFont);
        p.setPen(color);

        p.drawText(w - 38, yOffset + 143 - 30, 34, 26,
                   Qt::AlignRight | Qt::AlignBottom, suitSymbol());
    }

    if (m_card.score > 0) {
        QFont scFont = font();
        scFont.setPixelSize(12);
        scFont.setBold(true);
        p.setFont(scFont);
        p.setPen(QColor(255, 215, 0));

        QString scText = QString("%1分").arg(m_card.score);
        QRect textRect(0, yOffset + 143 - 22, w, 20);
        p.drawText(textRect, Qt::AlignCenter, scText);
    }
}