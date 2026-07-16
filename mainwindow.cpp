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
#include <QListWidgetItem>
#include <QStyle>
#include <QVideoSink>

using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
    , m_scanController(new ScanController(this))
{
    ui->setupUi(this);

    updateAvailablePorts();

    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->btnUnlock, &QPushButton::clicked, this, &MainWindow::onUnlockClicked);
    connect(ui->btnHoming, &QPushButton::clicked, this, &MainWindow::onHomingClicked);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(ui->btnSelectJson, &QPushButton::clicked, this, &MainWindow::onSelectJsonClicked);
    connect(ui->btnMove, &QPushButton::clicked, this, &MainWindow::onMoveClicked);
    connect(ui->btnResetAutoMode, &QPushButton::clicked, this, &MainWindow::onResetAutoModeClicked);
    connect(ui->btnLoadProject, &QPushButton::clicked, this, &MainWindow::onLoadProjectClicked);
    connect(ui->comboImages, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onImageSelectionChanged);

    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);

    QDoubleValidator *coordValidator = new QDoubleValidator(-9999.0, 9999.0, 3, this);
    coordValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineMoveX->setValidator(coordValidator);
    ui->lineMoveY->setValidator(coordValidator);

    QDoubleValidator *zValidator = new QDoubleValidator(-100.0, 100.0, 3, this);
    zValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineMoveZ->setValidator(zValidator);

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

    if (ui->videoContainer && ui->videoContainer->videoSink()) {
        connect(ui->videoContainer->videoSink(), &QVideoSink::videoFrameChanged,
                this, &MainWindow::onNewVideoFrame);
    }

    connect(ui->tab_2, &QTabWidget::currentChanged, this, [this](int index) {
        if (ui->tab_2->tabText(index).toLower() == "scan") {
            scanner->startCamera();
        } else {
            scanner->stopCamera();
        }
    });

    connect(m_scanController, &ScanController::progressUpdated, this, &MainWindow::onScanProgressUpdated);
    connect(m_scanController, &ScanController::statusTextChanged, this, &MainWindow::onScanStatusTextChanged);
    connect(m_scanController, &ScanController::gcodeCommandReady, this, &MainWindow::onScanGcodeReady);
    connect(m_scanController, &ScanController::screenshotRequested, this, &MainWindow::onScanScreenshotRequested);
    connect(m_scanController, &ScanController::finished, this, &MainWindow::onScanFinished);

    connect(ui->btnSetLT, &QPushButton::clicked, this, &MainWindow::onSetLtClicked);
    connect(ui->btnSetRB, &QPushButton::clicked, this, &MainWindow::onSetRbClicked);
    connect(ui->btnSelectDir, &QPushButton::clicked, this, &MainWindow::onSelectDirClicked);
    connect(ui->btnStartScan, &QPushButton::clicked, this, &MainWindow::onStartScanClicked);
    connect(ui->btnStopScan, &QPushButton::clicked, this, &MainWindow::onStopScanClicked);

    ui->lineSavePath->setText(QCoreApplication::applicationDirPath() + "/Projects/TempScan");

    QDoubleValidator *stepValidator = new QDoubleValidator(1.0, 100.0, 1, this);
    stepValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineScanStep->setValidator(stepValidator);

    ui->lineScanStep->clear();
    ui->lineScanStep->setPlaceholderText("Введите шаг (мм)");
}

MainWindow::~MainWindow() {
    if (serial->isOpen()) {
        serial->close();
    }
    delete ui;
}

void MainWindow::onNewVideoFrame() {
    if (!ui->videoContainer || !ui->videoContainer->videoSink()) return;

    QVideoFrame frame = ui->videoContainer->videoSink()->videoFrame();
    if (frame.isValid()) {
        if (!frame.isMapped()) {
            if (frame.map(QVideoFrame::ReadOnly)) {
                m_lastCameraFrame = frame.toImage().copy();

                frame.unmap();
            } else {
                qWarning() << "[Камера]: Не удалось спроецировать видеокадр в ОЗУ.";
            }
        } else {
            m_lastCameraFrame = frame.toImage().copy();
        }
    }
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
            ui->logConsole->addItem("[Система]: Порт успешно открыт.");
            ui->btnConnect->setText("Отключиться");
            statusTimer->start(100);

            ui->comboPorts->setProperty("connectionStatus", "connected");
            ui->comboPorts->style()->unpolish(ui->comboPorts);
            ui->comboPorts->style()->polish(ui->comboPorts);
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось открыть выбранный COM-порт!");
        }
    } else {
        statusTimer->stop();
        serial->close();
        ui->btnConnect->setText("Подключиться");
        ui->logConsole->addItem("[Система]: Соединение закрыто.");

        ui->comboPorts->setProperty("connectionStatus", "disconnected");
        ui->comboPorts->style()->unpolish(ui->comboPorts);
        ui->comboPorts->style()->polish(ui->comboPorts);
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

        if (trimmedLine.contains("MPos") || trimmedLine.contains("WPos") ||
            trimmedLine.startsWith("<") || trimmedLine.endsWith(">") ||
            trimmedLine == "ok" || trimmedLine.contains("ok"))
        {
            if (trimmedLine.contains("MPos") || trimmedLine.contains("WPos")) {
                parseStatusString(trimmedLine);
            }

            if ((trimmedLine == "ok" || trimmedLine.contains("ok")) && isAutoMode) {
                if (currentPointIndex >= route.size()) {
                    isAutoMode = false;
                    ui->btnMove->setEnabled(true);

                    ui->logConsole->clear();
                    ui->logConsole->addItem("[Система]: Все точки из файла успешно выполнены!");
                }
            }
            continue;
        }

        if (isAutoMode) {
            continue;
        }

        ui->logConsole->addItem("[GRBL]: " + trimmedLine);
        ui->logConsole->scrollToBottom();
    }
}

void MainWindow::onUnlockClicked() { if (serial->isOpen()) serial->write("$X\n"); }
void MainWindow::onHomingClicked() { if (serial->isOpen()) serial->write("$H\n"); }
void MainWindow::onResetClicked() { if (serial->isOpen()) { char cmd = 0x18; serial->write(&cmd, 1); } }

void MainWindow::onMoveClicked() {
    if (!serial->isOpen()) {
        ui->logConsole->addItem("[Система]: Ошибка. Станок не подключен!");
        return;
    }

    QString gcode;

    if (isAutoMode) {
        // --- АВТОМАТИЧЕСКИЙ РЕЖИМ (БЕЗ ИЗМЕНЕНИЙ) ---
        if (currentPointIndex < route.size()) {
            Point p = route[currentPointIndex];
            QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();

            gcode = QString("G90 G1 X%1 Y%2 F%3\n").arg(p.x).arg(p.y).arg(strF);

            if (currentPointIndex < routeButtons.size() && routeButtons[currentPointIndex]) {
                routeButtons[currentPointIndex]->setStyleSheet(
                    "QPushButton { background-color: #ffe0b2; border: none; text-align: left; padding: 6px 10px; font-size: 13px; font-weight: bold; }"
                    );
            }

            currentPointIndex++;
            ui->btnMove->setEnabled(false);

            serial->write(gcode.toUtf8());
        }
    } else {
        // --- РУЧНОЙ РЕЖИМ (ДОБАВЛЕНА ОСЬ Z) ---
        QString gMode = ui->radioAbs->isChecked() ? "G90" : "G91";

        // Считываем значения осей (если пусто — заменяем на "0")
        QString strX = ui->lineMoveX->text().isEmpty() ? "0" : ui->lineMoveX->text();
        QString strY = ui->lineMoveY->text().isEmpty() ? "0" : ui->lineMoveY->text();

        // ЧИТАЕМ ОСЬ Z ИЗ ТВОЕГО ПОЛЯ lineMoveZ
        QString strZ = ui->lineMoveZ->text().isEmpty() ? "0" : ui->lineMoveZ->text();

        QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();

        // Заменяем запятые на точки для стандартов G-кода
        strX.replace(",", ".");
        strY.replace(",", ".");
        strZ.replace(",", ".");

        // Формируем команду перемещения по трем осям: X, Y и Z!
        gcode = QString("%1 G1 X%2 Y%3 Z%4 F%5\n").arg(gMode, strX, strY, strZ, strF);

        ui->logConsole->addItem("[Передача -> GRBL]: " + gcode.trimmed());
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
    routeButtons.clear();

    for (size_t i = 0; i < route.size(); ++i) {
        QString textRow = QString("[Точка №%1]: X = %2, Y = %3")
                              .arg(route[i].id)
                              .arg(route[i].x, 0, 'f', 3)
                              .arg(route[i].y, 0, 'f', 3);

        QPushButton *btn = new QPushButton(textRow);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #ffcccc;"
            "  border: none;"
            "  text-align: left;"
            "  padding: 6px 10px;"
            "  font-size: 13px;"
            "}"
            "QPushButton:hover { background-color: #ffb3b3; }"
            );
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        connect(btn, &QPushButton::clicked, this, [this, i]() {

            if (i == (currentPointIndex - 1) && !ui->btnMove->isEnabled()) {
                return;
            }

            for (size_t k = 0; k < route.size(); ++k) {

                if (k >= i || route[k].status == PointStatus::Pending) {
                    route[k].status = PointStatus::Pending;

                    if (k < routeButtons.size() && routeButtons[k]) {
                        routeButtons[k]->setStyleSheet(
                            "QPushButton { background-color: #ffcccc; border: none; text-align: left; padding: 6px 10px; font-size: 13px; }"
                            );
                    }
                }
            }

            currentPointIndex = i;
            ui->btnMove->setEnabled(true);
            onMoveClicked();
        });

        QListWidgetItem *item = new QListWidgetItem(ui->logConsole);
        item->setSizeHint(QSize(0, 30));
        ui->logConsole->addItem(item);
        ui->logConsole->setItemWidget(item, btn);

        routeButtons.push_back(btn);
    }

    ui->btnMove->setEnabled(true);
}

void MainWindow::onResetAutoModeClicked() {
    if (!isAutoMode && routeButtons.empty()) {
        ui->logConsole->clear();
        ui->logConsole->addItem("[Система]: Готов к ручному вводу координат.");
        return;
    }

    isAutoMode = false;
    currentPointIndex = 0;

    ui->logConsole->clear();
    routeButtons.clear();

    ui->btnMove->setEnabled(true);

    ui->logConsole->addItem("[Система]: Автоматический режим сброшен. Консоль переведена в ручное ЧПУ-управление.");
    ui->logConsole->scrollToBottom();

    qDebug() << "[Система]: Маршрут JSON успешно выгружен оператором.";
}


void MainWindow::onSetLtClicked() {
    ui->lineLtX->setText(QString::number(wPosX, 'f', 3));
    ui->lineLtY->setText(QString::number(wPosY, 'f', 3));
    qDebug() << "[Сканирование]: Задан LT:" << wPosX << "," << wPosY;
}

void MainWindow::onSetRbClicked() {
    ui->lineRbX->setText(QString::number(wPosX, 'f', 3));
    ui->lineRbY->setText(QString::number(wPosY, 'f', 3));
    qDebug() << "[Сканирование]: Задан RB:" << wPosX << "," << wPosY;
}

void MainWindow::onSelectDirClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для кадров сканирования", ui->lineSavePath->text());
    if (!dir.isEmpty()) {
        ui->lineSavePath->setText(dir);
    }
}

void MainWindow::onStartScanClicked() {
    if (ui->lineLtX->text().isEmpty() || ui->lineRbX->text().isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Пожалуйста, зафиксируйте точки LT и RB перед стартом!");
        return;
    }

    // Проверяем шаг
    QString stepText = ui->lineScanStep->text().trimmed();
    if (stepText.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Необходимо ввести шаг сканирования!");
        ui->lineScanStep->setFocus();
        return;
    }

    // Читаем высоту Z (если поле пустое — по умолчанию едем на Z = 0.0)
    double zHeight = 0.0;
    if (ui->lineMoveZ && !ui->lineMoveZ->text().isEmpty()) {
        QString zText = ui->lineMoveZ->text().trimmed();
        zText.replace(",", ".");
        zHeight = zText.toDouble();
    }

    double step = stepText.toDouble();
    double pxPerMm = ui->linePixelDensity->text().toDouble();
    QString saveDir = ui->lineSavePath->text();
    QPointF lt(ui->lineLtX->text().toDouble(), ui->lineLtY->text().toDouble());
    QPointF rb(ui->lineRbX->text().toDouble(), ui->lineRbY->text().toDouble());

    // Передаем zHeight пятым параметром в setupSession
    if (m_scanController->setupSession(lt, rb, step, pxPerMm, zHeight, saveDir)) {
        ui->btnStartScan->setEnabled(false);
        ui->btnStopScan->setEnabled(true);
        m_scanController->start();
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сгенерировать очередь перемещений.");
    }
}

void MainWindow::onStopScanClicked() {
    m_scanController->stop();
    ui->btnStartScan->setEnabled(true);
    ui->btnStopScan->setEnabled(false);
}

// === ОБРАБОТЧИКИ СИГНАЛОВ СКАНИРОВАНИЯ ===

void MainWindow::onScanProgressUpdated(int current, int total) {
    ui->progressScan->setMaximum(total);
    ui->progressScan->setValue(current);
}

void MainWindow::onScanStatusTextChanged(const QString &text) {
    // Формируем строчку: показывает статус шага и текущие координаты станка X, Y, Z
    QString fullStatus = QString("%1 | Текущее положение: X: %2, Y: %3, Z: %4")
                             .arg(text)
                             .arg(wPosX, 0, 'f', 3)
                             .arg(wPosY, 0, 'f', 3)
                             .arg(grbl_Z, 0, 'f', 3);

    ui->lblScanStatus->setText(fullStatus);
}

void MainWindow::onScanGcodeReady(const QString &gcode) {
    if (serial->isOpen()) {
        serial->write(gcode.toUtf8());
        qDebug() << "[Отправка G-кода сканирования]:" << gcode.trimmed();
    }
}

void MainWindow::onScanScreenshotRequested(const QPointF &coord) {
    m_scanController->captureAndSaveFrame(m_lastCameraFrame, coord);
}

void MainWindow::onScanFinished() {
    ui->btnStartScan->setEnabled(true);
    ui->btnStopScan->setEnabled(false);
    QMessageBox::information(this, "Сканирование", "Все сектора платы успешно сохранены!");
}

void MainWindow::parseStatusString(const QString &statusStr) {
    QString statusText = "Unknown";
    double x = 0.0, y = 0.0;
    double z = 0.0; // Локальная переменная для координаты Z
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

        if (coords.size() >= 3) {
            z = coords[2].toDouble();
            grbl_Z = z;
        }

        // Обновляем вывод в верхнее поле координат (теперь с Z!)
        QString coordText = QString("Статус: %1 | %2: X: %3  Y: %4  Z: %5")
                                .arg(machineStatus).arg(posType)
                                .arg(wPosX, 0, 'f', 3)
                                .arg(wPosY, 0, 'f', 3)
                                .arg(grbl_Z, 0, 'f', 3);
        ui->lineEditCoordinates->setText(coordText);

        // Взаимодействие со сканером (Без изменений!)
        if (m_scanController->isScanningActive() && machineStatus.toLower() == "idle") {
            m_scanController->handleTargetReached();
        }

        // Твоя исходная логика автообхода при получении IDLE (Без изменений!)
        if (isAutoMode && machineStatus.toLower() == "idle") {

            if (currentPointIndex > 0) {
                size_t finishedIdx = currentPointIndex - 1;

                double diffX = std::abs(wPosX - route[finishedIdx].x);
                double diffY = std::abs(wPosY - route[finishedIdx].y);

                if (diffX < 0.05 && diffY < 0.05) {
                    route[finishedIdx].status = PointStatus::Completed;

                    if (finishedIdx < routeButtons.size() && routeButtons[finishedIdx]) {
                        routeButtons[finishedIdx]->setStyleSheet(
                            "QPushButton { background-color: #ccffcc; border: none; text-align: left; padding: 6px 10px; font-size: 13px; }"
                            );
                    }

                    if (currentPointIndex < route.size()) {
                        if (!ui->btnMove->isEnabled()) {
                            ui->btnMove->setEnabled(true);
                            onMoveClicked();
                        }
                    }
                }
            }
        }
    }
}

void MainWindow::onLoadProjectClicked() {
    QString defaultPath = "C:/Users/ven/Documents/FlyProbeCncGui/Projects";

    if (!QDir(defaultPath).exists()) {
        defaultPath = QCoreApplication::applicationDirPath() + "/Projects";
        QDir().mkpath(defaultPath);
    }

    QString projectDir = QFileDialog::getExistingDirectory(
        this,
        "Выбрать папку проекта ЧПУ",
        defaultPath
        );

    if (projectDir.isEmpty()) return;

    ProjectParser parser;
    QString errorMsg;
    m_projectData = parser.scanProjectDir(projectDir, errorMsg);

    if (!errorMsg.isEmpty()) {
        QMessageBox::warning(this, "Ошибка проекта", errorMsg);
        return;
    }

    ui->comboImages->blockSignals(true);
    ui->comboImages->clear();

    for (const QString &path : m_projectData.imagePaths) {
        ui->comboImages->addItem(QFileInfo(path).fileName());
    }

    ui->comboImages->blockSignals(false);

    if (!m_projectData.imagePaths.isEmpty()) {
        ui->comboImages->setCurrentIndex(0);
        onImageSelectionChanged(0);
    } else {
        ui->listPins->clear();
        ui->listPins->addItem("[Система]: В папке 'Виды' отсутствуют JPG файлы.");
    }
}

// ИСПРАВЛЕНИЕ ОШИБКИ 2: Берем данные точек напрямую из КЭША без чтения диска!
void MainWindow::onImageSelectionChanged(int index) {
    if (index < 0 || index >= m_projectData.imagePaths.size()) return;

    ProjectParser parser;
    QString activeImagePath = m_projectData.imagePaths[index];

    ui->probeGraphicsView->loadImage(activeImagePath);

    QString targetId = parser.extractIndexFromJpgBinary(activeImagePath);
    qDebug() << "[Интерфейс]: Запрос кэша для ID слоя:" << targetId;

    ui->listPins->clear();

    // Забираем данные из карты кэша слоев, созданного при парсинге проекта
    if (m_projectData.layersCache.contains(targetId)) {
        m_loadedPins = m_projectData.layersCache.value(targetId);

        ui->probeGraphicsView->displayPins(m_loadedPins);

        for (const PinData &pin : m_loadedPins) {
            ui->listPins->addItem(pin.name);
        }
    } else {
        ui->listPins->addItem("[Система]: Нет точек для этого слоя в кэше.");
    }
}
