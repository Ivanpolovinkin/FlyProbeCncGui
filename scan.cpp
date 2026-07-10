#include "scan.h"
#include <QDebug>

Scan::Scan(QVideoWidget *videoWidget, QObject *parent)
    : QObject(parent)
    , m_videoWidget(videoWidget)
    , m_camera(nullptr)
    , m_mediaSession(nullptr)
{
    QCameraDevice defaultDevice = QMediaDevices::defaultVideoInput();

    // ИСПРАВЛЕНО: В Qt 6 для проверки пустого устройства используется isNull()
    if (defaultDevice.isNull()) {
        qDebug() << "[Scan]: Камера по умолчанию не найдена! Проверьте подключение.";
        return;
    }

    qDebug() << "[Scan]: Используем камеру по умолчанию:" << defaultDevice.description();

    m_camera = new QCamera(defaultDevice, this);
    m_mediaSession = new QMediaCaptureSession(this);

    m_mediaSession->setCamera(m_camera);
    m_mediaSession->setVideoOutput(m_videoWidget);
}

Scan::~Scan() {
    stopCamera();
}

void Scan::startCamera() {
    if (m_camera) {
        m_camera->start();
        qDebug() << "[Scan]: Поток с камеры запущен.";
    }
}

void Scan::stopCamera() {
    if (m_camera && m_camera->isActive()) {
        m_camera->stop();
        qDebug() << "[Scan]: Поток с камеры остановлен.";
    }
}
