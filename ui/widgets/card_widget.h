#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QPixmap>
#include "core/card/card.h"

class CardWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentYOffset READ currentYOffset WRITE setCurrentYOffset)
    Q_PROPERTY(qreal dealProgress READ dealProgress WRITE setDealProgress)
public:
    explicit CardWidget(const Card& card, QWidget* parent = nullptr);
    ~CardWidget() override;
    Card getCard() const;
    void setCard(const Card& card);
    void setSelected(bool selected);
    bool isSelected() const;

    static QString cardImageFileName(const Card& card);

    int currentYOffset() const { return m_currentYOffset; }
    void setCurrentYOffset(int v);
    void setFlying(bool flying) { m_flying = flying; update(); }
    void setDealAnimationEnabled(bool enabled, bool revealFace = true);
    qreal dealProgress() const { return m_dealProgress; }
    void setDealProgress(qreal progress);
    void setDealTilt(qreal degrees);

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
    QPixmap m_pixmap;
    QPixmap m_backPixmap;
    bool m_dealAnimationEnabled = false;
    bool m_dealRevealFace = true;
    qreal m_dealProgress = 0.0;
    qreal m_dealTilt = 0.0;

    void paintDealCard();
    
    QColor textColor() const;
    QString suitSymbol() const;
    int pointFontSize() const;
    void loadPixmap();
};