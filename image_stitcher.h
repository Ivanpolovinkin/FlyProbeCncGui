#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QPointF>
#include <QVector>

// Структура, описывающая один найденный кадр и его метаданные
struct GridTile {
    QString filePath;   // Полный путь к файлу снимка
    QPointF coordinate; // Извлеченная координата (X, Y) в мм
};

class ImageStitcher : public QObject {
    Q_OBJECT
public:
    explicit ImageStitcher(QObject *parent = nullptr);
    ~ImageStitcher() override = default;

    /**
     * @brief Запуск сборки панорамы из папки с кропами
     * @param sourceDir Папка, в которой лежат сохраненные файлы вида X..._Y...png
     * @param step Физический шаг сканирования (мм)
     * @param pxPerMm Калибровочная плотность пикселей камеры (пикс/мм)
     * @param outErrorMessage Ссылка на строку, куда запишется ошибка, если что-то пойдет не так
     * @return QImage Готовое бесшовное изображение панорамы (или пустой QImage в случае неудачи)
     */
    QImage stitchFolder(const QString &sourceDir, double step, double pxPerMm, QString &outErrorMessage);

signals:
    // Сигнал для отправки текущего статуса склейки в интерфейс
    void statusUpdated(const QString &statusText);
};
