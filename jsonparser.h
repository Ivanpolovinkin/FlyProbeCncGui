#pragma once

#include <QString>
#include <vector>
#include "Point.h"

using namespace std;

class JsonParser {
public:
    // Статический метод, чтобы можно было вызывать без создания объекта класса
    static vector<Point> parseJsonFile(const QString &filePath, QString &errorString);
};


