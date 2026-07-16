#pragma once

#include <QString>
#include <QRectF>
#include <QVector>
#include <QMap>

// Структура для одной точки
struct PinData {
    QString name;
    QRectF rectOnScene;
};

// Изменяем результат сканирования проекта
struct ProjectScanResult {
    QVector<QString> imagePaths; // Пути к JPG файлам
    QString pointsFilePath;      // Путь к файлу Points

    // ДОБАВЛЯЕМ КЭШ: Сюда парсер сразу сложит ВСЕ точки, разбив их по слоям!
    // Пример: cache["1"] -> вектор точек первого слоя
    QMap<QString, QVector<PinData>> layersCache;
};
