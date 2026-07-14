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
    connect(ui->btnResetAutoMode, &QPushButton::clicked, this, &MainWindow::onResetAutoModeClicked);
    connect(ui->btnLoadProject, &QPushButton::clicked, this, &MainWindow::onLoadProjectClicked);
    connect(ui->comboImages, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onImageSelectionChanged);

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
            ui->logConsole->addItem("[Система]: Порт успешно открыт.");
            ui->btnConnect->setText("Отключиться");
            statusTimer->start(100);

            // Включаем зеленый цвет (меняем свойство на connected)
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

        // Возвращаем красный цвет (меняем свойство на disconnected)
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

        // 1. СЕРВИСНЫЙ ФИЛЬТР: Координаты и подтверждения GRBL
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

                    // Только по окончании всего файла выводим финальное сообщение в текстовом виде
                    ui->logConsole->clear(); // Очищаем список от кнопок, так как работа завершена
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
        if (currentPointIndex < route.size()) {
            Point p = route[currentPointIndex];
            QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();

            gcode = QString("G90 G1 X%1 Y%2 F%3\n").arg(p.x).arg(p.y).arg(strF);

            // КРАСИМ ТЕКУЩУЮ КНОПКУ В ОРАНЖЕВЫЙ ЦВЕТ СТРОГО ДО СДВИГА ИНДЕКСА
            if (currentPointIndex < routeButtons.size() && routeButtons[currentPointIndex]) {
                routeButtons[currentPointIndex]->setStyleSheet(
                    "QPushButton { background-color: #ffe0b2; border: none; text-align: left; padding: 6px 10px; font-size: 13px; font-weight: bold; }"
                    );
            }

            currentPointIndex++;
            ui->btnMove->setEnabled(false); // Блокируем кнопку, станок пошел работать

            serial->write(gcode.toUtf8());
        }
    } else {
        // Ручной режим
        QString gMode = ui->radioAbs->isChecked() ? "G90" : "G91";
        QString strX = ui->lineMoveX->text().isEmpty() ? "0" : ui->lineMoveX->text();
        QString strY = ui->lineMoveY->text().isEmpty() ? "0" : ui->lineMoveY->text();
        QString strF = ui->lineMoveF->text().isEmpty() ? "1500" : ui->lineMoveF->text();
        strX.replace(",", "."); strY.replace(",", ".");

        gcode = QString("%1 G1 X%2 Y%3 F%4\n").arg(gMode, strX, strY, strF);
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

            // Устанавливаем индекс строго на выбранную кнопку
            currentPointIndex = i;

            // Разблокируем кнопку управления, чтобы onMoveClicked сработал штатно
            ui->btnMove->setEnabled(true);
            onMoveClicked(); // Метод окрасит строго ТЕКУЩУЮ точку в оранжевый цвет
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
    // 1. Если автомат и так не был активен, просто очищаем лог на всякий случай
    if (!isAutoMode && routeButtons.empty()) {
        ui->logConsole->clear();
        ui->logConsole->addItem("[Система]: Готов к ручному вводу координат.");
        return;
    }

    // 2. Выключаем режим автоматического обхода
    isAutoMode = false;
    currentPointIndex = 0;

    // 3. Полностью очищаем память и интерфейс от интерактивных кнопок траектории
    ui->logConsole->clear();
    routeButtons.clear(); // Очищаем вектор указателей

    // 4. Возвращаем кнопку ручного движения в активное состояние
    ui->btnMove->setEnabled(true);

    // 5. Выводим приветственное системное сообщение, подтверждающее ручной режим
    ui->logConsole->addItem("[Система]: Автоматический режим сброшен. Консоль переведена в ручное ЧПУ-управление.");
    ui->logConsole->scrollToBottom();

    qDebug() << "[Система]: Маршрут JSON успешно выгружен оператором.";
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

            if (currentPointIndex > 0) {
                size_t finishedIdx = currentPointIndex - 1;

                // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Проверяем, что станок ДЕЙСТВИТЕЛЬНО доехал до координат этой точки!
                double diffX = std::abs(wPosX - route[finishedIdx].x);
                double diffY = std::abs(wPosY - route[finishedIdx].y);

                if (diffX < 0.05 && diffY < 0.05) {
                    route[finishedIdx].status = PointStatus::Completed;

                    // КРАСИМ КНОПКУ В ЗЕЛЕНЫЙ ЦВЕТ ТОЛЬКО ПО ПРИБЫТИЮ НА ТЕКУЩИЕ КООРДИНАТЫ
                    if (finishedIdx < routeButtons.size() && routeButtons[finishedIdx]) {
                        routeButtons[finishedIdx]->setStyleSheet(
                            "QPushButton { background-color: #ccffcc; border: none; text-align: left; padding: 6px 10px; font-size: 13px; }"
                            );
                    }

                    // ШАГАЕМ ДАЛЬШЕ: Переходим к следующей точке только если текущая успешно завершена
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
    // 1. Задаем приоритетный путь для разработки
    QString defaultPath = "C:/Users/ven/Documents/FlyProbeCncGui/Projects";

    // Если папки по этому абсолютному пути не существует (например, на другом ПК),
    // то откатываемся на папку "Projects" рядом с исполняемым файлом программы
    if (!QDir(defaultPath).exists()) {
        defaultPath = QCoreApplication::applicationDirPath() + "/Projects";

        // Создаем её автоматически для удобства, если её ещё нет
        QDir().mkpath(defaultPath);
    }

    // 2. Открываем проводник сразу в целевой папке
    QString projectDir = QFileDialog::getExistingDirectory(
        this,
        "Выбрать папку проекта ЧПУ",
        defaultPath
        );

    if (projectDir.isEmpty()) return;

    // 3. Сканируем выбранную папку проекта с помощью нашего парсера
    ProjectParser parser;
    QString errorMsg;
    m_projectData = parser.scanProjectDir(projectDir, errorMsg);

    if (!errorMsg.isEmpty()) {
        QMessageBox::warning(this, "Ошибка проекта", errorMsg);
        return;
    }

    // Блокируем сигналы комбобокса, чтобы при его очистке не вызывалась лишняя отрисовка сцены
    ui->comboImages->blockSignals(true);
    ui->comboImages->clear();

    // 4. Заполняем выпадающий список comboImages именами найденных JPG-файлов
    for (const QString &path : m_projectData.imagePaths) {
        ui->comboImages->addItem(QFileInfo(path).fileName());
    }

    ui->comboImages->blockSignals(false);

    // 5. Если изображения найдены — принудительно выбираем первое и отрисовываем его
    if (!m_projectData.imagePaths.isEmpty()) {
        ui->comboImages->setCurrentIndex(0);
        onImageSelectionChanged(0);
    } else {
        ui->listPins->clear();
        ui->listPins->addItem("[Система]: В папке 'Виды' отсутствуют JPG файлы.");
    }
}

void MainWindow::onImageSelectionChanged(int index) {
    if (index < 0 || index >= m_projectData.imagePaths.size()) return;

    ProjectParser parser;
    QString activeImagePath = m_projectData.imagePaths[index];

    // 1. Загружаем выбранный JPG файл на графический холст ProbeView
    ui->probeGraphicsView->loadImage(activeImagePath);

    // 2. Читаем бинарные метаданные JPG и вытаскиваем ID слоя (C++ аналог index_parser)
    QString targetIndex = parser.extractIndexFromJpgBinary(activeImagePath);
    qDebug() << "[Интерфейс]: Из бинарного JPG получен ID слоя:" << targetIndex;

    // Очищаем текстовый список точек перед новой загрузкой
    ui->listPins->clear();

    // 3. Если файл Points существует и ID слоя найден — парсим JSON-базу данных точек
    if (!m_projectData.pointsFilePath.isEmpty() && !targetIndex.isEmpty()) {
        bool ok = false;
        // Читаем точки из JSON файла Points по нашему ID слоя
        m_loadedPins = parser.parseJsonPointsForId(m_projectData.pointsFilePath, targetIndex, ok);

        if (ok && !m_loadedPins.isEmpty()) {
            // Накладываем интерактивные кружки разметки на сцену поверх фото платы
            ui->probeGraphicsView->displayPins(m_loadedPins);

            // Выводим список найденных точек в правое текстовое поле listPins
            for (const PinData &pin : m_loadedPins) {
                ui->listPins->addItem(pin.name);
            }
        } else {
            ui->listPins->addItem("[Система]: Для данного слоя точек разметки в JSON не найдено.");
        }
    } else {
        ui->listPins->addItem("[Система]: Файл 'Points' не найден или в JPG отсутствуют ID-метаданные.");
    }
}
