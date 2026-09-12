#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QPixmap>
#include "core/card.h"

class CardWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentYOffset READ currentYOffset WRITE setCurrentYOffset)
public:
    explicit CardWidget(const Card& card, QWidget* parent = nullptr);
    Card getCard() const;
    void setSelected(bool selected);
    bool isSelected() const;

    static QString cardImageFileName(const Card& card);

    int currentYOffset() const { return m_currentYOffset; }
    void setCurrentYOffset(int v);
    void setFlying(bool flying) { m_flying = flying; update(); }

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

    QColor textColor() const;
    QString suitSymbol() const;
    int pointFontSize() const;
    void loadPixmap();
};