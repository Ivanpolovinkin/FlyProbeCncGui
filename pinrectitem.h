#pragma once

#include <QGraphicsEllipseItem>
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>

class PinRectItem : public QGraphicsEllipseItem {
public:
    // Конструктор принимает имя точки и её границы на сцене
    PinRectItem(const QString &name, const QRectF &rect);

    QString getName() const { return m_name; }

    // Геттер и сеттер для физических координат ЧПУ (в мм)
    void setCNCPos(const QPointF &pos) { m_cncPos = pos; }
    QPointF getCNCPos() const { return m_cncPos; }

    // Метод переключения подсветки (hover или выделение из списка)
    void setHighlighted(bool status);

protected:
    // Переопределяем события мыши для интерактива
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString m_name;
    QPointF m_cncPos;
    bool m_isHighlighted = false;
    bool m_isBound = false;
};
