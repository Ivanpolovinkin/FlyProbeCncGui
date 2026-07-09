#include "jsonparser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

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

    // ИЗМЕНЕНО: Регулярное выражение теперь ищет только те ключи, которые заканчиваются на _7
    QRegularExpression keyRegex("\"(D\\d+_7)\"\\s*:\\s*\\{");
    QRegularExpressionMatchIterator it = keyRegex.globalMatch(jsonText);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString key = match.captured(1);
        if (dotsObj.contains(key) && !orderedKeys.contains(key)) {
            orderedKeys.append(key);
        }
    }

    int currentId = 1;

    for (const QString &key : orderedKeys) {
        // ДОПОЛНИТЕЛЬНАЯ ПРОВЕРКА: Пропускаем ключ, если он не оканчивается на _7
        if (!key.endsWith("_7")) continue;

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
                    route.push_back(p);
                }
            }
        }
    }

    if (route.empty()) {
        errorString = "Точки, заканчивающиеся на _7, не найдены!";
    } else {
        errorString = "";
    }

    return route;
}
