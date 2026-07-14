#include "probe_view.h"

ProbeView::ProbeView(QWidget *parent)
    : QGraphicsView(parent)
{
    // Настраиваем поведение вьювера
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    // Создаем дефолтную сцену
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
}

ProbeView::~ProbeView() {
    // Чистить сцену вручную не нужно, так как m_scene — дочерний объект QGraphicsView (this)
}

void ProbeView::createScene(const QPixmap &pixmap) {
    m_scene->clear(); // Полностью очищает все элементы со сцены и удаляет их из памяти
    m_pins.clear();

    // Задаем границы сцены по размеру картинки
    m_scene->setSceneRect(0, 0, pixmap.width(), pixmap.height());

    // Добавляем фоновое изображение платы
    m_pixmapItem = m_scene->addPixmap(pixmap);
    m_pixmapItem->setZValue(-1); // Картинка всегда на заднем плане
}

void ProbeView::addPin(const QString &name, const QRectF &rect) {
    if (!m_scene) return;

    PinRectItem *pin = new PinRectItem(name, rect);
    m_scene->addItem(pin);
    m_pins.append(pin);
}

void ProbeView::clearPins() {
    for (auto pin : m_pins) {
        m_scene->removeItem(pin);
        delete pin;
    }
    m_pins.clear();
}

void ProbeView::wheelEvent(QWheelEvent *event) {
    // Логика зума (коэффициент 1.05 при прокрутке вверх, 1/1.05 при прокрутке вниз)
    double zoomInFactor = 1.05;
    double zoomOutFactor = 1.0 / zoomInFactor;

    if (event->angleDelta().y() > 0) {
        scale(zoomInFactor, zoomInFactor);
        m_zoomFactor *= zoomInFactor;
    } else {
        scale(zoomOutFactor, zoomOutFactor);
        m_zoomFactor *= zoomOutFactor;
    }
}

void ProbeView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        // Перетаскивание правой кнопкой мыши имитирует зажатие левой кнопки для Drag-режима
        setDragMode(QGraphicsView::ScrollHandDrag);
        QMouseEvent fakeEvent(event->type(), event->localPos(), event->screenPos(),
                              Qt::LeftButton, event->buttons() | Qt::LeftButton, event->modifiers());
        QGraphicsView::mousePressEvent(&fakeEvent);
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

void ProbeView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        QMouseEvent fakeEvent(event->type(), event->localPos(), event->screenPos(),
                              Qt::LeftButton, event->buttons() & ~Qt::LeftButton, event->modifiers());
        QGraphicsView::mouseReleaseEvent(&fakeEvent);
        setDragMode(QGraphicsView::NoDrag);
        setCursor(Qt::CrossCursor); // Возвращаем перекрестие
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void ProbeView::loadImage(const QString &imagePath) {
    QPixmap pixmap(imagePath);
    if (!pixmap.isNull()) {
        createScene(pixmap);
    }
}

void ProbeView::displayPins(const QVector<PinData> &pins) {
    // 1. Очищаем старые точки перед отрисовкой новых
    clearPins();

    // 2. Добавляем новые точки на сцену
    for (const PinData &pin : pins) {
        addPin(pin.name, pin.rectOnScene);
    }
}
