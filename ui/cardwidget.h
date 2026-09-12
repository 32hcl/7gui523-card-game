#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QPixmap>
#include "core/card.h"

class CardWidget : public QWidget {
    Q_OBJECT
public:
    explicit CardWidget(const Card& card, QWidget* parent = nullptr);
    Card getCard() const;
    void setSelected(bool selected);
    bool isSelected() const;

    static QString cardImageFileName(const Card& card);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    Card m_card;
    bool m_selected = false;
    QPixmap m_pixmap;

    QColor textColor() const;
    QString suitSymbol() const;
    int pointFontSize() const;
    void loadPixmap();
};