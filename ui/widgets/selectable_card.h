#pragma once
#include <QWidget>
#include <QPixmap>
#include "core/card/card.h"

class SelectableCardWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelectableCardWidget(const Card& card, const QString& imagePath, QWidget* parent = nullptr);
    Card card() const { return m_card; }
    bool isSelected() const { return m_selected; }
    void setSelected(bool s) { m_selected = s; update(); }

signals:
    void clicked(SelectableCardWidget* self);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    Card m_card;
    QPixmap m_pixmap;
    bool m_selected = false;
};