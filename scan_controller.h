#pragma once

#include <QObject>
#include <QTimer>
#include <QImage>
#include <QPointF>
#include <QVector>

// Структура для одной физической точки в очереди команд сканирования
struct ScanCommand {
    QPointF coordinate; // Координата (X, Y) для отправки в GRBL в мм
    int rowIndex = 0;   // Индекс строки в сетке сканирования
    int colIndex = 0;   // Индекс колонки в сетке сканирования
};

// Структура, описывающая всю сессию сканирования
struct ScanSession {
    QPointF topLeft;       // Точка LT (мм)
    QPointF bottomRight;   // Точка RB (мм)
    double step = 0.0;    // Физический шаг сканирования (мм)
    double pxPerMm = 45.3; // Плотность пикселей камеры (калибровка)
    double zHeight = 10.0;
    QString saveDirectory; // Директория, куда складывать кропы кадров

    QVector<ScanCommand> queue; // Сгенерированная очередь команд перемещения
};

class ScanController : public QObject {
    Q_OBJECT
public:
    explicit ScanController(QObject *parent = nullptr);
    ~ScanController() override = default;

    // Инициализация параметров сессии и генерация траектории
    bool setupSession(const QPointF &lt, const QPointF &rb, double step, double density, double zHeight, const QString &savePath);

    // Запуск процесса сканирования по очереди команд
    void start();

    // Экстренная остановка процесса оператором
    void stop();

    // Сигнал от MainWindow: GRBL прибыл в координаты и перешел в режим Idle
    void handleTargetReached();

    // Возвращает, идет ли сканирование в данный момент
    bool isScanningActive() const { return m_isActive; }

    // Метод сохранения переданного кадра
    void captureAndSaveFrame(const QImage &rawFrame, const QPointF &coord);

signals:
    // Сигнал интерфейсу об изменении прогресса (текущий индекс, всего точек)
    void progressUpdated(int current, int total);

    // Сигнал интерфейсу об изменении текстового статуса сканера
    void statusTextChanged(const QString &text);

    // Сигнал в MainWindow для отправки сырой G-код команды в порт GRBL
    void gcodeCommandReady(const QString &gcode);

    // Сигнал запроса скриншота у главного окна (передает координату точки)
    void screenshotRequested(const QPointF &coord);

    // Сигнал об успешном завершении всего сканирования
    void finished();

private slots:
    // Срабатывает, когда таймер стабилизации картинки завершил отсчет
    void onStabilizationTimeout();

private:
    // Построение змейки обхода по двум точкам
    void generateSnakeRoute();

    // Переход к выполнению текущей команды по индексу m_currentIndex
    void executeCurrentCommand();

    ScanSession m_session;          // Данные сессии
    int m_currentIndex = -1;        // Текущий индекс выполняемой команды
    bool m_isActive = false;

    bool m_isWaitingForTarget = false;
    // Флаг активного процесса сканирования
    QTimer *m_stabilizationTimer;   // Таймер для устранения размытия кадра камеры
};
