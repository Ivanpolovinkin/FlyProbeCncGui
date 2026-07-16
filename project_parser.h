#pragma once

#include <QString>
#include <QDir>
#include <QVector>
#include <QMap>
#include "project_data.h"

class ProjectParser {
public:
    ProjectParser() = default;

    // Сканирует директорию проекта и кэширует JSON
    ProjectScanResult scanProjectDir(const QString &projectDir, QString &outError);

    // Достает ID слоя из бинарника JPG
    QString extractIndexFromJpgBinary(const QString &imagePath);
};
