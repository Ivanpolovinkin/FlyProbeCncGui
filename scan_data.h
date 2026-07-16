#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

// Структура для одной физической точки в очереди команд сканирования
struct ScanCommand {
    QPointF coordinate; // Координата (X, Y) для отправки в GRBL в мм
    int rowIndex = 0;   // Индекс строки в сетке сканирования
    int colIndex = 0;   // Индекс колонки в сетке сканирования
};

// Структура, описывающая всю сессию сканирования
struct ScanSession {
    QPointF topLeft;       // Точка LT (мм)
    QPointF bottomRight;   // Точка RB (мм)
    double step = 10.0;    // Физический шаг сканирования (мм)
    double pxPerMm = 45.3; // Плотность пикселей камеры (калибровка)
    QString saveDirectory; // Директория, куда складывать кропы кадров

    QVector<ScanCommand> queue; // Сгенерированная очередь команд перемещения
};
