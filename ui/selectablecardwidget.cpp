#include "selectablecardwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QImage>

SelectableCardWidget::SelectableCardWidget(const Card& card, const QString& path, QWidget* parent)
    : QWidget(parent), m_card(card)
{
    QImage img(path);
    if (img.isNull()) return;
    img = img.convertToFormat(QImage::Format_ARGB32);

    int minX = img.width(), minY = img.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(row[x]) > 16) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX >= 0) {
        QImage trimmed = img.copy(minX, minY, maxX - minX + 1, maxY - minY + 1);
        m_pixmap = QPixmap::fromImage(trimmed);
    } else {
        m_pixmap = QPixmap::fromImage(img);
    }
}

void SelectableCardWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_pixmap.isNull()) { p.fillRect(rect(), Qt::white); return; }

    p.drawPixmap(rect(), m_pixmap);

    if (m_selected) {
        p.setPen(QPen(QColor(255, 215, 0), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}

void SelectableCardWidget::mousePressEvent(QMouseEvent*) {
    emit clicked(this);
}