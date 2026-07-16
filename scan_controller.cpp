#include "scan_controller.h"
#include <QDir>
#include <QDebug>
#include <cmath>

ScanController::ScanController(QObject *parent)
    : QObject(parent), m_currentIndex(-1), m_isActive(false)
{
    m_stabilizationTimer = new QTimer(this);
    m_stabilizationTimer->setSingleShot(true);
    connect(m_stabilizationTimer, &QTimer::timeout, this, &ScanController::onStabilizationTimeout);
}

bool ScanController::setupSession(const QPointF &lt, const QPointF &rb, double step, double density, double zHeight, const QString &savePath) {
    if (m_isActive) {
        emit statusTextChanged("Ошибка: Нельзя изменить параметры во время активного сканирования.");
        return false;
    }

    m_session.topLeft = lt;
    m_session.bottomRight = rb;
    m_session.step = step;
    m_session.pxPerMm = density;
    m_session.zHeight = zHeight; // <-- Сохраняем высоту Z
    m_session.saveDirectory = savePath;

    generateSnakeRoute();
    return !m_session.queue.isEmpty();
}

void ScanController::generateSnakeRoute() {
    m_session.queue.clear();

    // Определяем истинные физические границы, за которые нельзя выходить
    double startX = std::min(m_session.topLeft.x(), m_session.bottomRight.x());
    double endX = std::max(m_session.topLeft.x(), m_session.bottomRight.x());
    double startY = std::min(m_session.topLeft.y(), m_session.bottomRight.y());
    double endY = std::max(m_session.topLeft.y(), m_session.bottomRight.y());

    // Вычисляем количество колонок и строк
    int cols = std::ceil((endX - startX) / m_session.step) + 1;
    int rows = std::ceil((endY - startY) / m_session.step) + 1;

    for (int r = 0; r < rows; ++r) {
        // Рассчитываем Y, но жестко ограничиваем его максимальной координатой endY!
        double y = startY + r * m_session.step;
        if (y > endY) {
            y = endY;
        }

        // Змейка: четные строки идут слева направо, нечетные — справа налево
        if (r % 2 == 0) {
            for (int c = 0; c < cols; ++c) {
                double x = startX + c * m_session.step;
                if (x > endX) x = endX; // Ограничение по X

                m_session.queue.append({QPointF(x, y), r, c});
            }
        } else {
            for (int c = cols - 1; c >= 0; --c) {
                double x = startX + c * m_session.step;
                if (x > endX) x = endX; // Ограничение по X

                m_session.queue.append({QPointF(x, y), r, c});
            }
        }
    }

    emit statusTextChanged(QString("Маршрут построен. Точек к обходу: %1").arg(m_session.queue.size()));
}

void ScanController::start() {
    if (m_session.queue.isEmpty()) {
        emit statusTextChanged("Ошибка: Очередь команд сканирования пуста.");
        return;
    }

    m_isActive = true;
    m_currentIndex = 0;

    // --- ОЧИСТКА ПАПКИ ПЕРЕД СТАРТОМ ---
    QDir dir(m_session.saveDirectory);
    if (dir.exists()) {
        emit statusTextChanged("Очистка папки от старых снимков...");

        // Получаем список всех файлов в папке (фильтруем только файлы, исключая папки "." и "..")
        QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &fileInfo : files) {
            QFile::remove(fileInfo.absoluteFilePath());
        }
        qDebug() << "[Сканер]: Папка очищена перед новым сканированием:" << m_session.saveDirectory;
    } else {
        QDir().mkpath(m_session.saveDirectory);
    }


    emit statusTextChanged("Сканирование запущено...");
    executeCurrentCommand();
}

void ScanController::stop() {
    if (!m_isActive) return;

    m_isActive = false;
    m_stabilizationTimer->stop();
    m_isWaitingForTarget = false;
    m_currentIndex = -1;

    emit statusTextChanged("Сканирование принудительно остановлено оператором.");
    emit progressUpdated(0, m_session.queue.size());
}

void ScanController::executeCurrentCommand() {
    if (!m_isActive) return;

    if (m_currentIndex < 0 || m_currentIndex >= m_session.queue.size()) {
        m_isActive = false;

        // БЕЗОПАСНОСТЬ: По окончании сканирования приподнимаем ось Z на безопасную высоту (например, Z=0 в машинных координатах или чуть выше)
        QString endGcode = "G90 G0 Z0\n";
        emit gcodeCommandReady(endGcode);

        emit statusTextChanged("Сканирование успешно завершено! Ось Z поднята.");
        emit finished();
        return;
    }

    emit progressUpdated(m_currentIndex + 1, m_session.queue.size());

    ScanCommand cmd = m_session.queue[m_currentIndex];
    emit statusTextChanged(QString("Движение к точке %1/%2 (X:%3, Y:%4, Z:%5)")
                               .arg(m_currentIndex + 1)
                               .arg(m_session.queue.size())
                               .arg(cmd.coordinate.x(), 0, 'f', 3)
                               .arg(cmd.coordinate.y(), 0, 'f', 3)
                               .arg(m_session.zHeight, 0, 'f', 3));

    m_isWaitingForTarget = true;

    QString gcode;
    if (m_currentIndex == 0) {
        // НА ПЕРВОЙ ТОЧКЕ: сначала едем по Z на рабочую высоту, затем позиционируем X и Y
        gcode = QString("G90 G0 Z%1\nG90 G1 X%2 Y%3 F1500\n")
                    .arg(m_session.zHeight, 0, 'f', 3)
                    .arg(cmd.coordinate.x(), 0, 'f', 3)
                    .arg(cmd.coordinate.y(), 0, 'f', 3);
    } else {
        // На остальных точках: движемся строго в плоскости XY, сохраняя Z
        gcode = QString("G90 G1 X%1 Y%2 F1500\n")
                    .arg(cmd.coordinate.x(), 0, 'f', 3)
                    .arg(cmd.coordinate.y(), 0, 'f', 3);
    }

    emit gcodeCommandReady(gcode);
}

void ScanController::handleTargetReached() {
    if (!m_isActive || !m_isWaitingForTarget) return;

    m_isWaitingForTarget = false;

    emit statusTextChanged(QString("Точка достигнута. Стабилизация кадра..."));
    m_stabilizationTimer->start(750);
}

void ScanController::onStabilizationTimeout() {
    if (!m_isActive) return;

    ScanCommand cmd = m_session.queue[m_currentIndex];
    emit screenshotRequested(cmd.coordinate);

    m_currentIndex++;
    executeCurrentCommand();
}

void ScanController::captureAndSaveFrame(const QImage &rawFrame, const QPointF &coord) {
    if (rawFrame.isNull()) {
        qWarning() << "[Контроллер сканирования]: Передан пустой кадр!";
        return;
    }

    int cropSize = std::round(m_session.step * m_session.pxPerMm);

    int cropX = (rawFrame.width() - cropSize) / 2;
    int cropY = (rawFrame.height() - cropSize) / 2;

    cropX = std::max(0, std::min(cropX, rawFrame.width() - cropSize));
    cropY = std::max(0, std::min(cropY, rawFrame.height() - cropSize));

    QImage croppedFrame = rawFrame.copy(cropX, cropY, cropSize, cropSize);

    QString fileName = QString("X%1_Y%2.png")
                           .arg(coord.x(), 0, 'f', 3)
                           .arg(coord.y(), 0, 'f', 3);

    QString fullPath = m_session.saveDirectory + "/" + fileName;
    croppedFrame.save(fullPath, "PNG");

    qDebug() << "[Контроллер сканирования]: Успешно сохранен кадр камеры:" << fileName;
}
