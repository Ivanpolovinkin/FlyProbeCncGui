#pragma once

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QPushButton>
#include <QTimer>
#include <QImage>
#include <QPointF>
#include <QVector>
#include <QVideoFrame>

#include "Point.h"
#include "scan.h"
#include "project_parser.h"
#include "scan_controller.h"
#include "image_stitcher.h"

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
    // Твои исходные слоты
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
    void onSetLtClicked();
    void onSetRbClicked();
    void onSelectDirClicked();
    void onStartScanClicked();
    void onStopScanClicked();
    void onStitchClicked();

    void onScanProgressUpdated(int current, int total);
    void onScanStatusTextChanged(const QString &text);
    void onScanGcodeReady(const QString &gcode);
    void onScanScreenshotRequested(const QPointF &coord);
    void onScanFinished();

    void onNewVideoFrame();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;

    double wPosX = 0.0;
    double wPosY = 0.0;
    double grbl_Z = 0.0;
    QString machineStatus = "Unknown";

    ProjectScanResult m_projectData;

    vector<Point> route;
    vector<QPushButton*> routeButtons;
    QVector<PinData> m_loadedPins;

    size_t currentPointIndex = 0;
    bool isAutoMode = false;
    QTimer *statusTimer;

    Scan *scanner;
    ScanController *m_scanController;
    QImage m_lastCameraFrame;

    void updateAvailablePorts();
    void parseStatusString(const QString &statusStr);
    ImageStitcher *m_stitcher = nullptr;

};
