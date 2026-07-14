#include "pinrectitem.h"
#include <QColor>

PinRectItem::PinRectItem(const QString &name, const QRectF &rect)
    : QGraphicsEllipseItem(rect), m_name(name), m_cncPos(0.0, 0.0)
{
    // Настраиваем внешний вид по умолчанию (как в Python: темно-зеленый, толщина 5)
    setPen(QPen(Qt::darkGreen, 5));
    setZValue(1);

    // ВКЛЮЧАЕМ отслеживание наведения мыши
    setAcceptHoverEvents(true);
}

void PinRectItem::setHighlighted(bool status) {
    m_isHighlighted = status;
    if (m_isHighlighted) {
        // Подсвеченный статус — ярко-зеленый
        setPen(QPen(Qt::green, 5));
    } else {
        // Обычный статус — темно-зеленый
        setPen(QPen(Qt::darkGreen, 5));
    }
}

void PinRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    setHighlighted(true);
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void PinRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    setHighlighted(false);
    QGraphicsEllipseItem::hoverLeaveEvent(event);
}

void PinRectItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Здесь мы в будущем сгенерируем сигнал для центрирования дерева UI на этой точке
        setHighlighted(true);
    }
    QGraphicsEllipseItem::mouseDoubleClickEvent(event);
}
