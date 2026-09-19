#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QPixmap>
#include "core/card/card.h"

class CardWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentYOffset READ currentYOffset WRITE setCurrentYOffset)
    Q_PROPERTY(qreal dealProgress READ dealProgress WRITE setDealProgress)
    Q_PROPERTY(qreal cardTilt READ cardTilt WRITE setCardTilt)
    Q_PROPERTY(bool faceUp READ faceUp WRITE setFaceUp)
public:
    explicit CardWidget(const Card& card, QWidget* parent = nullptr);
    Card getCard() const;
    void setSelected(bool selected);
    bool isSelected() const;

    static QString cardImageFileName(const Card& card);
    static QPixmap cardBackPixmap();

    int currentYOffset() const { return m_currentYOffset; }
    void setCurrentYOffset(int v);
    void resetToIdle() { m_selected = false; m_currentYOffset = 21; update(); }
    void setFlying(bool flying) { m_flying = flying; update(); }
    void setDealAnimationEnabled(bool enabled, bool revealFace = true);
    void setDealTilt(qreal degrees);

    qreal dealProgress() const { return m_dealProgress; }
    void setDealProgress(qreal v);
    qreal cardTilt() const { return m_cardTilt; }
    void setCardTilt(qreal v);
    bool faceUp() const { return m_faceUp; }
    void setFaceUp(bool v);

    int tiltSign() const { return m_tiltSign; }
    void setTiltSign(int v) { m_tiltSign = v; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    Card m_card;
    bool m_selected = false;
    bool m_flying = false;
    int  m_currentYOffset = 21;
    qreal m_dealProgress = 0.0;
    bool m_dealAnimationEnabled = false;
    bool m_dealRevealFace = true;
    qreal m_dealTilt = 0.0;
    qreal m_cardTilt = 0.0;
    bool m_faceUp = true;
    int m_tiltSign = 1;
    QPixmap m_pixmap;

    QColor textColor() const;
    QString suitSymbol() const;
    int pointFontSize() const;
    void loadPixmap();
    void paintDealCard();
};