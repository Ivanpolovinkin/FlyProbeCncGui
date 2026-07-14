#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QVector>
#include "pinrectitem.h"
#include "project_data.h"

class ProbeView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ProbeView(QWidget *parent = nullptr);
    ~ProbeView() override;

    // --- ДОБАВЬ ЭТИ ДВА МЕТОДА ДЛЯ СВЯЗКИ С MAINWINDOW ---
    void loadImage(const QString &imagePath);
    void displayPins(const QVector<PinData> &pins);

    // Существующие методы
    void createScene(const QPixmap &pixmap);
    void addPin(const QString &name, const QRectF &rect);
    void clearPins();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
    QVector<PinRectItem*> m_pins;
    double m_zoomFactor = 1.0;
};
