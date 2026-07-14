#pragma once

#include <QString>
#include <QDir>
#include <QVector>
#include <QJsonObject>
#include "project_data.h"

class ProjectParser {
public:
    ProjectParser() = default;

    // Сканирует директорию проекта, находит JPG и путь к JSON-файлу Points
    ProjectScanResult scanProjectDir(const QString &projectDir, QString &outError);

    // Считывает бинарные данные JPG и вытаскивает ID из метаданных (C++ аналог index_parser)
    QString extractIndexFromJpgBinary(const QString &imagePath);

    // Парсит JSON-файл "Points" и собирает координаты точек для выбранного ID слоя
    QVector<PinData> parseJsonPointsForId(const QString &pointsFilePath, const QString &targetId, bool &ok);

private:
    // Вспомогательный метод для рекурсивного обхода JSON-дерева в поисках ключа "Views"
    void searchViewsInJson(const QJsonObject &obj, const QString &targetId, QVector<PinData> &outPins, int &counter);

    // Вспомогательный метод для создания PinData из объекта с ключами L, T, R, B
    PinData createPinFromViewObject(const QJsonObject &viewObj, const QString &name, bool &ok);
};
