#include "mainwindow.h"
#include "jsonparser.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QRegularExpression>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QDir>
#include <QTimer>

using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
{
    ui->setupUi(this);

    // Сканируем порты при старте приложения
    updateAvailablePorts();

    // Связываем сигналы кнопок из UI дизайна с методами (слотами)
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->btnUnlock, &QPushButton::clicked, this, &MainWindow::onUnlockClicked);
    connect(ui->btnHoming, &QPushButton::clicked, this, &MainWindow::onHomingClicked);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(ui->btnSelectJson, &QPushButton::clicked, this, &MainWindow::onSelectJsonClicked);
    connect(ui->btnMove, &QPushButton::clicked, this, &MainWindow::onMoveClicked);

    // Асинхронное чтение из COM-порта
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);

    QDoubleValidator *coordValidator = new QDoubleValidator(-9999.0, 9999.0, 3, this);
    coordValidator->setNotation(QDoubleValidator::StandardNotation);

    ui->lineMoveX->setValidator(coordValidator);
    ui->lineMoveY->setValidator(coordValidator);

    QIntValidator *speedValidator = new QIntValidator(1, 20000, this);
    ui->lineMoveF->setValidator(speedValidator);

    // Инициализация фонового таймера опроса координат
    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, [this]() {
        if (serial->isOpen()) {
            char cmd = '?';
            serial->write(&cmd, 1);
        }
    });
}

MainWindow::~MainWindow() {
    if (serial->isOpen()) {
        serial->close();
    }
    delete ui;
}

void MainWindow::updateAvailablePorts() {
    ui->comboPorts->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ui->comboPorts->addItem(info.portName());
    }
}

void MainWindow::onConnectClicked() {
    if (!serial->isOpen()) {
        serial->setPortName(ui->comboPorts->currentText());
        serial->setBaudRate(QSerialPort::Baud115200);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);

        if (serial->open(QIODevice::ReadWrite)) {
            ui->logConsole->append("[Система]: Порт успешно открыт. Ожидание инициализации...");
            ui->btnConnect->setText("Отключиться");

            // ЗАПУСК ТАЙМЕРА: опрашиваем станок каждые 100 мс (10 раз в сек)
            statusTimer->start(100);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть выбранный COM-порт!");
        }
    } else {
        // ОСТАНОВКА ТАЙМЕРА при отключении
        statusTimer->stop();

        serial->close();
        ui->btnConnect->setText("Подключиться");
        ui->logConsole->append("[Система]: Соединение закрыто.");
    }
}

// МЕТОД onStatusClicked ПОЛНОСТЬЮ УДАЛЕН ОТСЮДА

void MainWindow::readData() {
    QByteArray data = serial->readAll();
    if (data.isEmpty()) return;

    QString response = QString::fromUtf8(data);
    QStringList lines = response.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;

        // 1. ПЕРЕХВАТ КООРДИНАТНЫХ СТРОК И ИХ ОБРУБКОВ (Остается без изменений)
        if (trimmedLine.contains("MPos") || trimmedLine.contains("WPos") || trimmedLine.endsWith(">")) {

            if (trimmedLine.contains("MPos") || trimmedLine.contains("WPos")) {
                parseStatusString(trimmedLine);
            }

            // Если "ok" прилип к статусной строке
            if (trimmedLine.contains("ok") && isAutoMode) {
                if (currentPointIndex >= route.size()) {
                    ui->logConsole->append("[Система]: Все точки успешно пройдены!");
                    isAutoMode = false;
                    ui->btnMove->setEnabled(true);
                }
            }
        }
        // 2. ОТВЕТЫ НА КОМАНДЫ (Чистые ok, ошибки, приветствия)
        else {
            // Если пришел ответ "ok" на команду перемещения
            if (trimmedLine.contains("ok")) {

                // Выводим подтверждение в лог-панель, как ты и просил
                ui->logConsole->append("[GRBL]: " + trimmedLine);

                if (isAutoMode) {
                    if (currentPointIndex >= route.size()) {
                        ui->logConsole->append("[Система]: Все точки успешно пройдены!");
                        isAutoMode = false;
                        ui->btnMove->setEnabled(true);
                    }
                }
            }
            // Ошибки, алармы и прочие текстовые сообщения
            else {
                ui->logConsole->append("[GRBL]: " + trimmedLine);
            }
        }
    }
}

void MainWindow::onUnlockClicked() {
    if (serial->isOpen()) {
        ui->logConsole->append("[Передача -> GRBL]: $X");
        serial->write("$X\n");
    }
}

void MainWindow::onHomingClicked() {
    if (serial->isOpen()) {
        ui->logConsole->append("[Передача -> GRBL]: $H");
        serial->write("$H\n");
    }
}

void MainWindow::onResetClicked() {
    if (serial->isOpen()) {
        ui->logConsole->append("[Передача -> GRBL]: Мягкий сброс (0x18)");
        char cmd = 0x18;
        serial->write(&cmd, 1);
    }
}

void MainWindow::onMoveClicked() {
    if (!serial->isOpen()) {
        ui->logConsole->append("[Система]: Ошибка. Станок не подключен!");
        return;
    }

    QString gcode;

    if (isAutoMode) {
        if (currentPointIndex < route.size()) {
            Point p = route[currentPointIndex];
            QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();

            gcode = QString("G90 G1 X%1 Y%2 F%3\n").arg(p.x).arg(p.y).arg(strF);

            ui->logConsole->append(QString("[Автомат]: Отправка точки №%1/%2: %3")
                                       .arg(currentPointIndex + 1).arg(route.size()).arg(gcode.trimmed()));

            currentPointIndex++;

            // Блокируем кнопку перемещения до подтверждения Idle через таймер
            ui->btnMove->setEnabled(false);

            serial->write(gcode.toUtf8());
        }
    } else {
        // Ручной режим работы
        QString gMode = ui->radioAbs->isChecked() ? "G90" : "G91";
        QString strX = ui->lineMoveX->text().isEmpty() ? "0" : ui->lineMoveX->text();
        QString strY = ui->lineMoveY->text().isEmpty() ? "0" : ui->lineMoveY->text();
        QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();

        strX.replace(",", ".");
        strY.replace(",", ".");

        gcode = QString("%1 G1 X%2 Y%3 F%4\n").arg(gMode, strX, strY, strF);
        ui->logConsole->append("[Передача -> GRBL]: " + gcode.trimmed());
        serial->write(gcode.toUtf8());
    }
}

void MainWindow::onSelectJsonClicked() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString targetDirPath = appDir + "/json_trajectories";

    QDir targetDir(targetDirPath);
    if (!targetDir.exists()) {
        targetDir.mkpath(".");
    }

    QString defaultPath = QDir::toNativeSeparators(targetDirPath);

    QString filePath = QFileDialog::getOpenFileName(
        this, "Выберите файл конфигурации точек", defaultPath, "All Files (*)"
        );

    if (filePath.isEmpty()) return;

    ui->logConsole->append("[Система]: Выбран файл: " + filePath);

    QString errorMsg;
    route = JsonParser::parseJsonFile(filePath, errorMsg);

    if (!errorMsg.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", errorMsg);
        isAutoMode = false;
        ui->btnMove->setEnabled(true);
        ui->btnMove->setText("Переместить (G1)");
        return;
    }

    currentPointIndex = 0;
    isAutoMode = true;

    ui->logConsole->append(QString("[Парсер]: Успешно загружена траектория. Считано точек: %1.").arg(route.size()));
    ui->logConsole->append("[Система]: Ожидание запуска. Нажмите кнопку перемещения для отправки Точки №1.");

    ui->btnMove->setEnabled(true);
    ui->btnMove->setText("Переместить (G1)");
}

void MainWindow::parseStatusString(const QString &statusStr) {
    // Реализация логики Python на Qt/C++ через split
    QString statusText = "Unknown";
    double x = 0.0, y = 0.0;

    // Определяем, какой тип позиции нам прилетел
    QString posType = statusStr.contains("MPos") ? "MPos" : "WPos";

    // Разбиваем строку по символу '|'
    QStringList parts = statusStr.split('|');
    if (parts.size() < 2) return; // Если строка побилась, выходим

    // 1. Извлекаем статус (убираем лишний '<' слева, если он там остался)
    statusText = parts[0];
    statusText.remove('<');
    if (statusText.contains("ok")) {
        statusText = statusText.split(" ").last(); // Отрезаем прилипший "ok "
    }
    machineStatus = statusText.trimmed();

    // 2. Ищем, в какой из частей лежат координаты (обычно во 2-й, индекс 1)
    QString coordPart;
    for (const QString &part : parts) {
        if (part.contains(posType)) {
            coordPart = part;
            break;
        }
    }

    if (coordPart.isEmpty()) return;

    // Отрезаем префикс "MPos:" или "WPos:"
    coordPart.remove(posType + ":");

    // Разбиваем оставшиеся цифры по запятой
    QStringList coords = coordPart.split(',');
    if (coords.size() >= 2) {
        x = coords[0].toDouble();
        y = coords[1].toDouble();

        wPosX = x;
        wPosY = y;

        // Выводим результат строго в нижнюю панель lineEditCoordinates
        QString coordText = QString("Статус: %1 | %2: X: %3  Y: %4")
                                .arg(machineStatus)
                                .arg(posType)
                                .arg(wPosX, 0, 'f', 3)
                                .arg(wPosY, 0, 'f', 3);

        ui->lineEditCoordinates->setText(coordText);

        // Логика автоматического перехода к следующей точке JSON траектории
        if (isAutoMode && machineStatus.toLower() == "idle") {
            if (currentPointIndex < route.size()) {
                if (!ui->btnMove->isEnabled()) {
                    ui->btnMove->setEnabled(true);
                    ui->logConsole->append(QString("[Система]: Точка достигнута. Кнопка готова к отправке Точки №%1.").arg(currentPointIndex + 1));
                }
            }
        }
    }
}
