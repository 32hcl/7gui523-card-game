#include "card_widget.h"
#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QPolygonF>
#include <QRadialGradient>
#include <QTransform>
#include <cmath>

CardWidget::CardWidget(const Card& card, QWidget* parent)
    : QWidget(parent), m_card(card), m_currentYOffset(21)
{
    setFixedSize(110, 180);   // 从 168 → 180，多出 12px 给阴影
    loadPixmap();
}

Card CardWidget::getCard() const
{
    return m_card;
}

void CardWidget::setCard(const Card& card)
{
    m_card = card;
    m_pixmap = QPixmap();
    loadPixmap();
    update();
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

void CardWidget::loadPixmap() {
    QString cardsDir = QCoreApplication::applicationDirPath() + "/cards/";
    QString fileName = cardImageFileName(m_card);
    QString fullPath = cardsDir + fileName;
    static QHash<QString, QPixmap> s_cache;
    auto it = s_cache.constFind(fullPath);
    if (it != s_cache.constEnd()) {
        m_pixmap = it.value();
    } else if (QFile::exists(fullPath)) {
        m_pixmap = QPixmap(fullPath);
        s_cache.insert(fullPath, m_pixmap);
    }
}

void CardWidget::setDealAnimationEnabled(bool enabled, bool revealFace)
{
    m_dealAnimationEnabled = enabled;
    m_dealRevealFace = revealFace;
    m_dealProgress = 0.0;
    if (enabled && m_backPixmap.isNull()) {
        m_backPixmap.load(QCoreApplication::applicationDirPath() + "/cards/card_back.png");
    }
    update();
}

void CardWidget::setDealProgress(qreal progress)
{
    m_dealProgress = qBound(qreal(0.0), progress, qreal(1.0));
    update();
}

void CardWidget::setDealTilt(qreal degrees)
{
    m_dealTilt = qBound(qreal(-10.0), degrees, qreal(10.0));
    update();
}

void CardWidget::paintDealCard()
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Keep the face readable after the edge-on midpoint (no mirrored text).
    constexpr qreal pi = 3.14159265358979323846;
    const qreal lift = std::sin(pi * m_dealProgress);
    const qreal scaleX = qMax(qreal(0.015), std::abs(std::cos(pi * m_dealProgress)));
    const bool showFace = m_dealRevealFace && m_dealProgress >= 0.5;

    // The temporary deal widget reserves 18 px around the 60 x 84 card.
    // Paint its ground shadow before transforming the card, so the shadow
    // remains visible while the face is edge-on.
    p.save();
    p.translate(width() / 2.0 + 3.0 * lift, height() / 2.0 + 41.0 + 5.0 * lift);
    p.scale(1.0 - 0.35 * lift, 0.24);
    QRadialGradient shadow(QPointF(0.0, 0.0), 33.0);
    shadow.setColorAt(0.0, QColor(0, 0, 0, qRound(115.0 - 80.0 * lift)));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawEllipse(QRectF(-33.0, -33.0, 66.0, 66.0));
    p.restore();

    p.translate(width() / 2.0, height() / 2.0);
    p.rotate(m_dealTilt * lift);
    p.scale(scaleX, 1.0);
    const QRectF cardRect(-30.0, -42.0, 60.0, 84.0);
    const qreal inset = 4.0 * lift;
    const QPolygonF source{cardRect.topLeft(), cardRect.topRight(),
                           cardRect.bottomRight(), cardRect.bottomLeft()};
    const QPolygonF projected{QPointF(-30.0 + inset, -42.0 + inset),
                              QPointF(30.0 - inset, -42.0 + inset),
                              QPointF(30.0, 42.0), QPointF(-30.0, 42.0)};
    QTransform perspective;
    if (QTransform::quadToQuad(source, projected, perspective)) {
        p.setWorldTransform(perspective, true);
    }
    const QPixmap& pixmap = showFace ? m_pixmap : m_backPixmap;
    if (!pixmap.isNull()) {
        p.drawPixmap(cardRect, pixmap, QRectF(pixmap.rect()));
        return;
    }

    // Missing assets must still show a back before the midpoint, never the face.
    p.setPen(QPen(QColor(35, 50, 75), 1.0));
    p.setBrush(showFace ? QColor(Qt::white) : QColor(45, 70, 120));
    p.drawRoundedRect(cardRect, 4.0, 4.0);
    QFont labelFont = font();
    labelFont.setPixelSize(13);
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(showFace ? textColor() : QColor(Qt::white));
    const QString label = showFace
        ? QString::fromStdString(m_card.point) + "\n" + suitSymbol()
        : QStringLiteral("\u25c6");
    p.drawText(cardRect, Qt::AlignCenter, label);
}

void CardWidget::paintEvent(QPaintEvent*)
{
    if (m_dealAnimationEnabled) {
        paintDealCard();
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int yOffset = m_currentYOffset;

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

    // 阴影紧贴卡片底部，宽度与卡片一致
    // 卡片升起时，阴影只升起一半
    constexpr int kRestY = 21;                                  // 未选中时的 yOffset
    const int liftCompensation = (kRestY - m_currentYOffset) / 2; // 未选=0，选中=9
    const int shadowTop = cardBottom + liftCompensation;

    constexpr int kShadowHeight = 4;
    if (shadowTop + kShadowHeight <= height()) {
        QLinearGradient grad(0, shadowTop, 0, shadowTop + kShadowHeight);
        grad.setColorAt(0.0, QColor(0, 0, 0, 180));
        grad.setColorAt(1.0, QColor(0, 0, 0, 0));
        QRect shadowRect(imgX, shadowTop, imgW, kShadowHeight);  // 宽度 = 卡片宽度
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
}