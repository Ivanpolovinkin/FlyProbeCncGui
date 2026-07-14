#include "project_parser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>

ProjectScanResult ProjectParser::scanProjectDir(const QString &projectDir, QString &outError) {
    ProjectScanResult result;
    QDir rootDir(projectDir);

    if (!rootDir.exists()) {
        outError = "Папка проекта не найдена.";
        return result;
    }

    // 1. Ищем JPG файлы в "Виды"
    QDir viewsDir(projectDir + "/Виды");
    if (!viewsDir.exists()) {
        outError = "Папка 'Виды' отсутствует внутри директории проекта.";
        return result;
    }

    QStringList imgFilters;
    imgFilters << "*.jpg";
    QFileInfoList imgFiles = viewsDir.entryInfoList(imgFilters, QDir::Files);
    for (const QFileInfo &fileInfo : imgFiles) {
        result.imagePaths.append(fileInfo.absoluteFilePath());
    }

    // 2. Ищем файл базы данных "Points" (без расширения) в "Контрольные точки"
    QString pointsFilePath = projectDir + "/Контрольные точки/Points";
    QFileInfo pointsFile(pointsFilePath);

    if (pointsFile.exists() && pointsFile.isFile()) {
        result.pointsFilePath = pointsFile.absoluteFilePath();
        qDebug() << "[Парсер]: Найдена база данных JSON (Points):" << result.pointsFilePath;
    } else {
        // Проверяем на всякий случай "Points.txt" или "Points.json"
        QFileInfo pointsJson(projectDir + "/Контрольные точки/Points.json");
        if (pointsJson.exists()) {
            result.pointsFilePath = pointsJson.absoluteFilePath();
        }
    }

    if (result.imagePaths.isEmpty()) {
        outError = "В папке 'Виды' не найдено JPG-изображений.";
    } else if (result.pointsFilePath.isEmpty()) {
        qWarning() << "[Парсер]: Внимание! Файл 'Points' не обнаружен.";
    }

    return result;
}

// C++ аналог твоего index_parser на байтах и регулярках
QString ProjectParser::extractIndexFromJpgBinary(const QString &imagePath) {
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[Парсер]: Не удалось открыть JPG для чтения байт:" << imagePath;
        return "";
    }

    // Читаем весь файл в массив байт (bytearray в Python)
    QByteArray imageBytes = file.readAll();
    file.close();

    // Быстро преобразуем в Latin1-строку для работы регулярного выражения без потери байт
    QString content = QString::fromLatin1(imageBytes);

    // Регулярное выражение для поиска {"ID" : ...}
    QRegularExpression re("\\{\"ID\"\\s*:\\s*(\\d+)\\}");
    QRegularExpressionMatchIterator it = re.globalMatch(content);

    QString lastId = "";
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        lastId = match.captured(1); // Запоминаем последнее совпадение (как indexes[-1] в Python)
    }

    qDebug() << "[Парсер]: Из бинарного файла JPG успешно вытащен слой ID:" << lastId;
    return lastId;
}

QVector<PinData> ProjectParser::parseJsonPointsForId(const QString &pointsFilePath, const QString &targetId, bool &ok) {
    QVector<PinData> pins;
    QFile file(pointsFilePath);

    if (!file.open(QIODevice::ReadOnly)) {
        ok = false;
        return pins;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(fileData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[Парсер]: Ошибка парсинга JSON Points:" << parseError.errorString();
        ok = false;
        return pins;
    }

    int autoNameCounter = 1;
    if (doc.isObject()) {
        // Запускаем рекурсивный обход JSON-дерева для извлечения точек слоя
        searchViewsInJson(doc.object(), targetId, pins, autoNameCounter);
    }

    ok = true;
    return pins;
}

// Рекурсивный обходчик JSON дерева (для универсальной поддержки любого уровня вложенности в Points)
void ProjectParser::searchViewsInJson(const QJsonObject &obj, const QString &targetId, QVector<PinData> &outPins, int &counter) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = it.key();
        QJsonValue val = it.value();

        if (val.isObject()) {
            QJsonObject subObj = val.toObject();

            // Если мы дошли до блока "Views"
            if (key == "Views" && subObj.contains(targetId)) {
                QJsonValue idVal = subObj.value(targetId);
                QString pinName = QString("Точка №%1").arg(counter++);

                // Случай А: Под ID лежит одиночный объект (как у "D1_1")
                if (idVal.isObject()) {
                    bool ok = false;
                    PinData pin = createPinFromViewObject(idVal.toObject(), pinName, ok);
                    if (ok) outPins.append(pin);
                }
                // Случай Б: Под ID лежит массив объектов (как у "С7")
                else if (idVal.isArray()) {
                    QJsonArray arr = idVal.toArray();
                    for (int i = 0; i < arr.size(); ++i) {
                        if (arr.at(i).isObject()) {
                            bool ok = false;
                            PinData pin = createPinFromViewObject(arr.at(i).toObject(), pinName, ok);
                            if (ok) outPins.append(pin);
                        }
                    }
                }
            } else {
                // Если это обычный объект, спускаемся глубже по дереву JSON
                searchViewsInJson(subObj, targetId, outPins, counter);
            }
        }
    }
}

// Извлечение координат L, T, R, B и формирование прямоугольника
PinData ProjectParser::createPinFromViewObject(const QJsonObject &viewObj, const QString &name, bool &ok) {
    PinData pin;
    if (viewObj.contains("L") && viewObj.contains("T") && viewObj.contains("R") && viewObj.contains("B")) {
        double left = viewObj.value("L").toDouble();
        double top = viewObj.value("T").toDouble();
        double right = viewObj.value("R").toDouble();
        double bottom = viewObj.value("B").toDouble();

        pin.name = name;
        // Переводим границы [L, T, R, B] в QRectF [X, Y, Width, Height]
        pin.rectOnScene = QRectF(left, top, right - left, bottom - top);
        ok = true;
    } else {
        ok = false;
    }
    return pin;
}
