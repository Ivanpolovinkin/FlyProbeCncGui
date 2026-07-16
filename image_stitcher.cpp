#include "image_stitcher.h"
#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QPainter>
#include <QDebug>
#include <cmath>
#include <limits>

ImageStitcher::ImageStitcher(QObject *parent) : QObject(parent) {}

QImage ImageStitcher::stitchFolder(const QString &sourceDir, double step, double pxPerMm, QString &outErrorMessage) {
    QDir dir(sourceDir);
    if (!dir.exists()) {
        outErrorMessage = "Указанная папка со снимками не существует.";
        return QImage();
    }

    emit statusUpdated("Анализ папки с кадрами...");

    // 1. Поиск всех PNG файлов в папке
    QStringList filters;
    filters << "*.png";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    if (fileList.isEmpty()) {
        outErrorMessage = "В папке не найдено ни одного кадра PNG для склейки.";
        return QImage();
    }

    // Регулярное выражение для извлечения координат из имени файла вида: X10.000_Y-20.500.png
    // Поддерживает целые, дробные числа, а также отрицательные значения координат
    QRegularExpression rx("X(-?\\d+(?:\\.\\d+)?)\\s*_\\s*Y(-?\\d+(?:\\.\\d+)?)\\.png");

    QVector<GridTile> tiles;
    double minX = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();

    // 2. Парсинг координат и поиск экстремумов
    for (const QFileInfo &fileInfo : fileList) {
        QRegularExpressionMatch match = rx.match(fileInfo.fileName());
        if (match.hasMatch()) {
            double x = match.captured(1).toDouble();
            double y = match.captured(2).toDouble();

            GridTile tile;
            tile.filePath = fileInfo.absoluteFilePath();
            tile.coordinate = QPointF(x, y);
            tiles.append(tile);

            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }

    if (tiles.isEmpty()) {
        outErrorMessage = "Не удалось распознать формат имен файлов (ожидается X[число]_Y[число].png).";
        return QImage();
    }

    emit statusUpdated(QString("Найдено секторов: %1. Расчет размеров полотна...").arg(tiles.size()));

    // 3. Вычисление физических и пиксельных габаритов
    double widthMm = (maxX - minX) + step;
    double heightMm = (maxY - minY) + step;

    int canvasWidth = std::round(widthMm * pxPerMm);
    int canvasHeight = std::round(heightMm * pxPerMm);
    int tileSize = std::round(step * pxPerMm);

    // Безопасное ограничение на создание изображения
    if (canvasWidth <= 0 || canvasHeight <= 0 || canvasWidth > 30000 || canvasHeight > 30000) {
        outErrorMessage = QString("Некорректные размеры итоговой панорамы (%1x%2 пикс). Проверьте настройки шага и плотности.").arg(canvasWidth).arg(canvasHeight);
        return QImage();
    }

    // Создаем пустое полотно панорамы с альфа-каналом (прозрачный фон)
    QImage canvas(canvasWidth, canvasHeight, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    // Настройки сглаживания для идеального стыка пиксель-в-пиксель
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    int processedCount = 0;

    // 4. Отрисовка кадров стык-в-стык на холсте
    for (const GridTile &tile : tiles) {
        QImage img(tile.filePath);
        if (img.isNull()) {
            qWarning() << "[Склейка]: Не удалось загрузить файл" << tile.filePath;
            continue;
        }

        // Если размер оригинального кропа чуть-чуть отличается, принудительно масштабируем под расчетный шаг
        if (img.width() != tileSize || img.height() != tileSize) {
            img = img.scaled(tileSize, tileSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        // Рассчитываем точное положение левого верхнего угла кадра на холсте
        int posX = std::round((tile.coordinate.x() - minX) * pxPerMm);

        int posY = std::round((tile.coordinate.y() - minY) * pxPerMm);

        // Рисуем кусочек на холсте
        painter.drawImage(posX, posY, img);

        processedCount++;
        emit statusUpdated(QString("Склейка: обработано %1 из %2 кадров...").arg(processedCount).arg(tiles.size()));
    }

    painter.end();

    emit statusUpdated(QString("Склейка завершена! Создано полотно %1x%2 пикселей.").arg(canvasWidth).arg(canvasHeight));
    return canvas;
}
