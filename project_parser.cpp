#include "project_parser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QDebug>

// Вспомогательная функция для глубокого поиска ВСЕХ блоков "Views" в JSON
void parseAllJsonLayers(const QJsonObject &obj, QMap<QString, QVector<PinData>> &cache, const QString &parentKey = "") {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = it.key();
        QJsonValue val = it.value();

        if (val.isObject()) {
            QJsonObject subObj = val.toObject();

            // Если нашли блок "Views"
            if (key == "Views") {
                // Имя компонента — это имя родительского объекта (например, "D1_1")
                QString pinName = parentKey.isEmpty() ? "Точка" : parentKey;

                // Пробегаем по всем ID слоев внутри этого Views (например, "1", "2")
                for (auto layerIt = subObj.begin(); layerIt != subObj.end(); ++layerIt) {
                    QString layerId = layerIt.key();
                    QJsonValue idVal = layerIt.value();

                    // Функция для создания PinData из L, T, R, B
                    auto createPin = [](const QJsonObject &vObj, const QString &name) -> PinData {
                        PinData p;
                        double left = vObj.value("L").toDouble();
                        double top = vObj.value("T").toDouble();
                        double right = vObj.value("R").toDouble();
                        double bottom = vObj.value("B").toDouble();
                        p.name = name;
                        p.rectOnScene = QRectF(left, top, right - left, bottom - top);
                        return p;
                    };

                    // Если под слоем лежит один объект
                    if (idVal.isObject()) {
                        PinData pin = createPin(idVal.toObject(), pinName);
                        cache[layerId].append(pin); // Кладываем в карту по ключу слоя!
                    }
                    // Если под слоем лежит массив объектов
                    else if (idVal.isArray()) {
                        QJsonArray arr = idVal.toArray();
                        for (int i = 0; i < arr.size(); ++i) {
                            if (arr.at(i).isObject()) {
                                QString multiName = (arr.size() > 1) ? QString("%1_%2").arg(pinName).arg(i + 1) : pinName;
                                PinData pin = createPin(arr.at(i).toObject(), multiName);
                                cache[layerId].append(pin);
                            }
                        }
                    }
                }
            } else {
                // Идем глубже по дереву, передавая имя текущего узла как имя компонента
                QString nextParentKey = (key == "Dots") ? parentKey : key;
                parseAllJsonLayers(subObj, cache, nextParentKey);
            }
        }
    }
}

QString ProjectParser::extractIndexFromJpgBinary(const QString &imagePath) {
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[Парсер]: Не удалось открыть JPG для чтения байт:" << imagePath;
        return "";
    }

    QByteArray imageBytes = file.readAll();
    file.close();

    QString content = QString::fromLatin1(imageBytes);

    QRegularExpression re("\\{\"ID\"\\s*:\\s*(\\d+)\\}");
    QRegularExpressionMatchIterator it = re.globalMatch(content);

    QString lastId = "";
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        lastId = match.captured(1);
    }

    qDebug() << "[Парсер]: Из бинарного файла JPG успешно вытащен слой ID:" << lastId;
    return lastId;
}

ProjectScanResult ProjectParser::scanProjectDir(const QString &projectDir, QString &outError) {
    ProjectScanResult result;
    QDir rootDir(projectDir);

    if (!rootDir.exists()) {
        outError = "Папка проекта не найдена.";
        return result;
    }

    // 1. Собираем JPG из "Виды"
    QDir viewsDir(projectDir + "/Виды");
    if (!viewsDir.exists()) {
        outError = "Папка 'Виды' отсутствует.";
        return result;
    }

    QFileInfoList imgFiles = viewsDir.entryInfoList(QStringList() << "*.jpg", QDir::Files);
    for (const QFileInfo &fileInfo : imgFiles) {
        result.imagePaths.append(fileInfo.absoluteFilePath());
    }

    // 2. Ищем файл базы данных "Points"
    QString pointsFilePath = projectDir + "/Контрольные точки/Points";
    QFileInfo pointsFile(pointsFilePath);

    if (pointsFile.exists() && pointsFile.isFile()) {
        result.pointsFilePath = pointsFile.absoluteFilePath();

        // --- ВОТ ЗДЕСЬ ПРОИСХОДИТ ОДНОКРАТНЫЙ ПАРСИНГ ВСЕГО JSON ---
        QFile file(result.pointsFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (doc.isObject()) {
                // Наполняем наш QMap layersCache всеми точками всех слоев разом!
                parseAllJsonLayers(doc.object(), result.layersCache, "");
                qDebug() << "[Парсер]: Успешно кэшировано слоев из JSON:" << result.layersCache.keys();
            }
        }
    }

    if (result.imagePaths.isEmpty()) {
        outError = "В папке 'Виды' не найдено JPG-изображений.";
    }

    return result;
}
