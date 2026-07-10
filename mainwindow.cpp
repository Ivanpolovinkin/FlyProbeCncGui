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
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>

using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
{
    ui->setupUi(this);

    updateAvailablePorts();

    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->btnUnlock, &QPushButton::clicked, this, &MainWindow::onUnlockClicked);
    connect(ui->btnHoming, &QPushButton::clicked, this, &MainWindow::onHomingClicked);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(ui->btnSelectJson, &QPushButton::clicked, this, &MainWindow::onSelectJsonClicked);
    connect(ui->btnMove, &QPushButton::clicked, this, &MainWindow::onMoveClicked);

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);

    QDoubleValidator *coordValidator = new QDoubleValidator(-9999.0, 9999.0, 3, this);
    coordValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineMoveX->setValidator(coordValidator);
    ui->lineMoveY->setValidator(coordValidator);

    QIntValidator *speedValidator = new QIntValidator(1, 20000, this);
    ui->lineMoveF->setValidator(speedValidator);

    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, [this]() {
        if (serial->isOpen()) {
            char cmd = '?';
            serial->write(&cmd, 1);
        }
    });
    scanner = new Scan(ui->videoContainer, this);

    connect(ui->tab_2, &QTabWidget::currentChanged, this, [this](int index) {
        if (ui->tab_2->tabText(index).toLower() == "scan") {
            scanner->startCamera();
        } else {
            scanner->stopCamera();
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
            ui->logConsole->append("[Система]: Порт успешно открыт.");
            ui->btnConnect->setText("Отключиться");
            statusTimer->start(100);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть выбранный COM-порт!");
        }
    } else {
        statusTimer->stop();
        serial->close();
        ui->btnConnect->setText("Подключиться");
        ui->logConsole->append("[Система]: Соединение закрыто.");
    }
}

void MainWindow::readData() {
    QByteArray data = serial->readAll();
    if (data.isEmpty()) return;

    QString response = QString::fromUtf8(data);
    QStringList lines = response.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;

        if (trimmedLine.contains("MPos") || trimmedLine.contains("WPos") || trimmedLine.endsWith(">")) {
            if (trimmedLine.contains("MPos") || trimmedLine.contains("WPos")) {
                parseStatusString(trimmedLine);
            }
            if (trimmedLine.contains("ok") && isAutoMode) {
                if (currentPointIndex >= route.size()) {
                    ui->logConsole->append("[Система]: Все точки из файла успешно выполнены!");
                    isAutoMode = false;
                    ui->btnMove->setEnabled(true);
                }
            }
        }
        else {
            if (trimmedLine.contains("ok")) {
                if (isAutoMode) {
                    if (currentPointIndex >= route.size()) {
                        ui->logConsole->append("[Система]: Все точки из файла успешно выполнены!");
                        isAutoMode = false;
                        ui->btnMove->setEnabled(true);
                    }
                }
            } else {
                ui->logConsole->append("[GRBL]: " + trimmedLine);
            }
        }
    }
}

void MainWindow::onUnlockClicked() { if (serial->isOpen()) serial->write("$X\n"); }
void MainWindow::onHomingClicked() { if (serial->isOpen()) serial->write("$H\n"); }
void MainWindow::onResetClicked() { if (serial->isOpen()) { char cmd = 0x18; serial->write(&cmd, 1); } }

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

            // КОД ОКРАШИВАНИЯ ОТСЮДА ПОЛНОСТЬЮ УДАЛЕН

            currentPointIndex++;
            ui->btnMove->setEnabled(false); // Блокируем кнопку до прибытия станка

            serial->write(gcode.toUtf8());
        }
    } else {
        // Ручной режим (без изменений)
        QString gMode = ui->radioAbs->isChecked() ? "G90" : "G91";
        QString strX = ui->lineMoveX->text().isEmpty() ? "0" : ui->lineMoveX->text();
        QString strY = ui->lineMoveY->text().isEmpty() ? "0" : ui->lineMoveY->text();
        QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();
        strX.replace(",", "."); strY.replace(",", ".");

        gcode = QString("%1 G1 X%2 Y%3 F%4\n").arg(gMode, strX, strY, strF);
        ui->logConsole->append("[Передача -> GRBL]: " + gcode.trimmed());
        serial->write(gcode.toUtf8());
    }
}
void MainWindow::onSelectJsonClicked() {
    QString targetPath = QCoreApplication::applicationDirPath() + "/json_trajectories";

    QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл конфигурации", targetPath, "All Files (*)");
    if (filePath.isEmpty()) return;

    QString errorMsg;
    route = JsonParser::parseJsonFile(filePath, errorMsg);

    if (!errorMsg.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", errorMsg);
        isAutoMode = false;
        return;
    }

    currentPointIndex = 0;
    isAutoMode = true;

    ui->logConsole->clear();

    // Отключаем переносы строк и убираем внутренние поля документа, чтобы краска ложилась вплотную к краям
    ui->logConsole->setLineWrapMode(QTextEdit::NoWrap);
    ui->logConsole->document()->setDocumentMargin(0);

    QTextCursor cursor(ui->logConsole->document());

    // Настраиваем формат блока для КРАСНОГО фона (на всю ширину)
    QTextBlockFormat redBlockFormat;
    redBlockFormat.setBackground(QColor("#ffcccc")); // Светло-красный
    // УДАЛЕНО: redBlockFormat.setPadding(2); — этот метод вызывал ошибку

    for (size_t i = 0; i < route.size(); ++i) {
        QString textRow = QString("[Точка №%1]: X = %2, Y = %3")
                              .arg(route[i].id)
                              .arg(route[i].x, 0, 'f', 3)
                              .arg(route[i].y, 0, 'f', 3);

        // Устанавливаем формат для текущей строки
        cursor.setBlockFormat(redBlockFormat);
        cursor.insertText(textRow);

        // Если это не последняя точка, создаем новый блок (строку)
        if (i < route.size() - 1) {
            cursor.insertBlock();
        }
    }

    ui->btnMove->setEnabled(true);
}

void MainWindow::parseStatusString(const QString &statusStr) {
    QString statusText = "Unknown";
    double x = 0.0, y = 0.0;
    QString posType = statusStr.contains("MPos") ? "MPos" : "WPos";

    QStringList parts = statusStr.split('|');
    if (parts.size() < 2) return;

    statusText = parts[0];
    statusText.remove('<');
    if (statusText.contains("ok")) statusText = statusText.split(" ").last();
    machineStatus = statusText.trimmed();

    QString coordPart;
    for (const QString &part : parts) {
        if (part.contains(posType)) { coordPart = part; break; }
    }
    if (coordPart.isEmpty()) return;

    coordPart.remove(posType + ":");
    QStringList coords = coordPart.split(',');
    if (coords.size() >= 2) {
        x = coords[0].toDouble();
        y = coords[1].toDouble();
        wPosX = x; wPosY = y;

        QString coordText = QString("Статус: %1 | %2: X: %3  Y: %4")
                                .arg(machineStatus).arg(posType)
                                .arg(wPosX, 0, 'f', 3).arg(wPosY, 0, 'f', 3);
        ui->lineEditCoordinates->setText(coordText);

        // ЛОГИКА АВТОМАТИЧЕСКОГО ОБХОДА ПРИ ПОЛУЧЕНИИ IDLE
        if (isAutoMode && machineStatus.toLower() == "idle") {

            // Проверяем: если станок только что доехал до точки — красим её в зелёный
            if (currentPointIndex > 0 && route[currentPointIndex - 1].status == PointStatus::Pending) {
                size_t finishedIdx = currentPointIndex - 1;
                route[finishedIdx].status = PointStatus::Completed;

                // Находим блок (строку) отработавшей точки по индексу
                QTextBlock block = ui->logConsole->document()->findBlockByNumber(static_cast<int>(finishedIdx));
                if (block.isValid()) {
                    QTextCursor cursor(block);

                    // Модифицируем формат строки (абзаца), чтобы залить её зелёным на 100% ширины
                    QTextBlockFormat greenBlockFormat = block.blockFormat();
                    greenBlockFormat.setBackground(QColor("#ccffcc")); // Салатовый

                    cursor.setBlockFormat(greenBlockFormat);
                }
            }

            // Автоматически шагаем дальше, если есть куда ехать
            if (currentPointIndex < route.size()) {
                if (!ui->btnMove->isEnabled()) {
                    ui->btnMove->setEnabled(true);
                    onMoveClicked(); // Запускает движение к следующей точке
                }
            }
        }
    }
}
