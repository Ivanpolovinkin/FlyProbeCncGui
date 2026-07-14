#pragma once

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QPushButton>

#include "Point.h"
#include "scan.h"
#include "project_parser.h"

using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Настройки подключения и ручные команды
    void onConnectClicked();
    void onUnlockClicked();
    void onHomingClicked();
    void onResetClicked();
    void readData();
    void onMoveClicked();
    void onSelectJsonClicked();
    void onResetAutoModeClicked();
    void onLoadProjectClicked();
    void onImageSelectionChanged(int index);

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;

    // Внутренние переменные для хранения координат станка
    double wPosX = 0.0;
    double wPosY = 0.0;
    QString machineStatus = "Unknown";

    ProjectScanResult m_projectData;

    vector<Point> route;
    vector<QPushButton*> routeButtons;
    QVector<PinData> m_loadedPins;

    size_t currentPointIndex = 0;
    bool isAutoMode = false;

    void updateAvailablePorts();
    void parseStatusString(const QString &statusStr);
    QTimer *statusTimer;
    Scan *scanner;
};
