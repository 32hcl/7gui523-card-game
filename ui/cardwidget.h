#pragma once

#include <QWidget>
#include <QMouseEvent>
#include "core/card.h"

class CardWidget : public QWidget {
    Q_OBJECT
public:
    explicit CardWidget(const Card& card, QWidget* parent = nullptr);
    Card getCard() const;
    void setSelected(bool selected);
    bool isSelected() const;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    Card m_card;
    bool m_selected = false;
    QColor textColor() const;
    QString suitSymbol() const;
    int pointFontSize() const;
};