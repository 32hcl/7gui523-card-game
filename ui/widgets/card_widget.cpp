#include "card_widget.h"
#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QCoreApplication>
#include <QFile>
#include <cmath>

CardWidget::CardWidget(const Card& card, QWidget* parent)
    : QWidget(parent), m_card(card), m_currentYOffset(21)
{
    setFixedSize(110, 168);
    loadPixmap();
}

Card CardWidget::getCard() const
{
    return m_card;
}

void CardWidget::setSelected(bool selected)
{
    if (m_selected == selected) return;
    m_selected = selected;

    QPropertyAnimation* cardAnim = new QPropertyAnimation(this, "currentYOffset");
    cardAnim->setDuration(150);
    cardAnim->setEasingCurve(QEasingCurve::OutCubic);
    cardAnim->setStartValue(m_currentYOffset);
    cardAnim->setEndValue(m_selected ? 3 : 21);
    cardAnim->start(QAbstractAnimation::DeleteWhenStopped);
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

void CardWidget::setDealProgress(qreal v)
{
    m_dealProgress = v;
    bool shouldFaceUp = (v >= 0.5);
    if (m_faceUp != shouldFaceUp) {
        m_faceUp = shouldFaceUp;
    }
    update();
}

void CardWidget::setCardTilt(qreal v)
{
    m_cardTilt = v;
    update();
}

void CardWidget::setFaceUp(bool v)
{
    m_faceUp = v;
    update();
}

void CardWidget::mousePressEvent(QMouseEvent*)
{
    setSelected(!m_selected);
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

QPixmap CardWidget::cardBackPixmap()
{
    static QPixmap s_back;
    if (s_back.isNull()) {
        QString backPath = QCoreApplication::applicationDirPath() + "/cards/card_back.png";
        if (QFile::exists(backPath)) {
            s_back = QPixmap(backPath);
        }
    }
    return s_back;
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

    qreal tilt = m_cardTilt;
    if (tilt == 0.0 && m_dealProgress > 0.0 && m_dealProgress < 1.0) {
        static const qreal pi = 3.14159265359;
        tilt = std::sin(pi * m_dealProgress) * 10.0 * m_tiltSign;
    }

    if (tilt != 0.0) {
        p.save();
        qreal cx = width() / 2.0;
        qreal cy = height() / 2.0;
        p.translate(cx, cy);
        p.rotate(tilt);
        p.translate(-cx, -cy);
    }

    const int w = width();
    const int yOffset = (m_dealProgress > 0.0) ? 0 : m_currentYOffset;

    bool inDeal = (m_dealProgress > 0.0 && m_dealProgress < 1.0);

    if (inDeal && !m_faceUp) {
        QPixmap back = cardBackPixmap();
        if (!back.isNull()) {
            QPixmap scaled = back.scaled(w - 8, 143,
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
            int imgX = (w - scaled.width()) / 2;
            p.drawPixmap(imgX, yOffset, scaled);
        } else {
            p.setPen(QPen(Qt::darkGray, 2));
            p.setBrush(QColor(40, 40, 80));
            p.drawRoundedRect(1, yOffset, w - 2, 143, 8, 8);
        }
        if (tilt != 0.0) p.restore();
        return;
    }

    int imgX = 0, imgY = yOffset, imgW = w, imgH = 143;

    if (!m_pixmap.isNull()) {
        QPixmap scaled = m_pixmap.scaled(w - 8, 143,
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        imgX = (w - scaled.width()) / 2;
        imgY = yOffset;
        imgW = scaled.width();
        imgH = scaled.height();

        p.drawPixmap(imgX, imgY, scaled);
    } else {
        imgX = 1;
        imgW = w - 2;
        imgH = 143;

        if (m_selected && !m_flying) {
            p.setPen(QPen(QColor(255, 200, 0), 3));
        } else {
            p.setPen(QPen(Qt::black, 2));
        }
        p.setBrush(Qt::white);
        p.drawRoundedRect(imgX, imgY, imgW, imgH, 8, 8);

        const QColor color = textColor();
        p.setPen(color);

        QFont ptFont = font();
        ptFont.setPixelSize(pointFontSize());
        ptFont.setBold(true);
        p.setFont(ptFont);

        const QString ptText = QString::fromStdString(m_card.point);
        p.drawText(6, imgY + 4, w - 12, 22, Qt::AlignLeft | Qt::AlignTop, ptText);

        if (m_card.point != "大鬼" && m_card.point != "小鬼") {
            QFont centerFont = font();
            centerFont.setPixelSize(34);
            p.setFont(centerFont);
            p.setOpacity(0.20);
            p.drawText(0, imgY, w, imgH, Qt::AlignCenter, suitSymbol());
            p.setOpacity(1.0);
        }

        QFont suitFont = font();
        suitFont.setPixelSize(16);
        suitFont.setBold(true);
        p.setFont(suitFont);
        p.setPen(color);

        p.drawText(w - 38, imgY + imgH - 30, 34, 26,
                   Qt::AlignRight | Qt::AlignBottom, suitSymbol());
    }

    int cardBottom = imgY + imgH;
    int shadowBottom = height() - 4;
    if (shadowBottom > cardBottom) {
        QLinearGradient grad(0, cardBottom, 0, shadowBottom);
        grad.setColorAt(0.0, QColor(0, 0, 0, 180));
        grad.setColorAt(1.0, QColor(0, 0, 0, 0));
        QRect shadowRect(4, cardBottom, w - 8, shadowBottom - cardBottom);
        p.fillRect(shadowRect, grad);
    }

    if (m_card.score > 0) {
        QFont scFont = font();
        scFont.setPixelSize(12);
        scFont.setBold(true);
        p.setFont(scFont);
        p.setPen(QColor(255, 215, 0));

        QString scText = QString("%1分").arg(m_card.score);
        QRect textRect(0, imgY + imgH - 22, w, 20);
        p.drawText(textRect, Qt::AlignCenter, scText);
    }

    if (tilt != 0.0) {
        p.restore();
    }
}