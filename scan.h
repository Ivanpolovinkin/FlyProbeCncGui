#ifndef SCAN_H
#define SCAN_H

#include <QObject>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoWidget>
#include <QMediaDevices>

class Scan : public QObject
{
    Q_OBJECT
public:
    explicit Scan(QVideoWidget *videoWidget, QObject *parent = nullptr);
    ~Scan();

    void startCamera(); // Метод запуска трансляции
    void stopCamera();  // Метод остановки трансляции

private:
    QCamera *m_camera;
    QMediaCaptureSession *m_mediaSession;
    QVideoWidget *m_videoWidget;
};

#endif // SCAN_H
