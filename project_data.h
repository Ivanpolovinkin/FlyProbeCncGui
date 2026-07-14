#pragma once

#include <QString>
#include <QRectF>
#include <QVector>

// Структура для представления одной контрольной точки на сцене (в пикселях)
struct PinData {
    QString name;
    QRectF rectOnScene; // Координаты прямоугольника: x, y, width, height
};

// Результат первичного сканирования выбранной папки проекта
struct ProjectScanResult {
    QVector<QString> imagePaths; // Полные пути ко всем найденным *.jpg в "Виды"
    QString pointsFilePath;      // Полный путь к файлу "Points" (если найден)
};
