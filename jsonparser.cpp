#include "jsonparser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

using namespace std;

vector<Point> JsonParser::parseJsonFile(const QString &filePath, QString &errorString) {
    vector<Point> route;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorString = "Не удалось открыть файл!";
        return route;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        errorString = "Ошибка структуры JSON: " + parseError.errorString();
        return route;
    }

    QJsonObject rootObj = doc.object();
    if (!rootObj.contains("Dots") || !rootObj["Dots"].isObject()) {
        errorString = "В файле отсутствует или некорректен блок 'Dots'!";
        return route;
    }

    QJsonObject dotsObj = rootObj["Dots"].toObject();

    QString jsonText = QString::fromUtf8(fileData);
    QStringList orderedKeys;

    // ИЗМЕНЕНО: Регулярное выражение теперь ищет ЛЮБЫЕ ключи, начинающиеся на D и содержащие цифры
    // Например: "D1", "D12_7", "D105"
    QRegularExpression keyRegex("\"(D\\d+(?:_\\d+)?)\"\\s*:\\s*\\{");
    QRegularExpressionMatchIterator it = keyRegex.globalMatch(jsonText);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString key = match.captured(1);
        if (dotsObj.contains(key) && !orderedKeys.contains(key)) {
            orderedKeys.append(key);
        }
    }

    // Если регулярка из-за форматирования текста дала сбой, делаем надежный запасной сборщик
    if (orderedKeys.isEmpty()) {
        QStringList allKeys = dotsObj.keys();
        for (const QString &key : allKeys) {
            if (key.startsWith("D")) {
                orderedKeys.append(key);
            }
        }
    }

    int currentId = 1;

    for (const QString &key : orderedKeys) {
        // Проверяем, что ключ действительно начинается на букву 'D'
        if (!key.startsWith("D")) continue;

        QJsonObject dotData = dotsObj[key].toObject();

        if (dotData.contains("Views") && dotData["Views"].isObject()) {
            QJsonObject viewsObj = dotData["Views"].toObject();

            if (viewsObj.contains("2")) {
                QJsonValue viewLayerValue = viewsObj["2"];
                QJsonObject targetView;

                if (viewLayerValue.isArray() && !viewLayerValue.toArray().isEmpty()) {
                    targetView = viewLayerValue.toArray().at(0).toObject();
                } else if (viewLayerValue.isObject()) {
                    targetView = viewLayerValue.toObject();
                }

                if (targetView.contains("L") && targetView.contains("T")) {
                    Point p;
                    p.id = currentId++;
                    p.x = targetView["L"].toDouble();
                    p.y = targetView["T"].toDouble();
                    // Поле p.status автоматически выставится в PointStatus::Pending из point.h

                    route.push_back(p);
                }
            }
        }
    }

    if (route.empty()) {
        errorString = "Точки, начинающиеся на букву 'D', в файле не найдены!";
    } else {
        errorString = "";
    }

    return route;
}
