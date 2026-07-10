#pragma once

enum class PointStatus {
    Pending,   // Ожидает (красный фон)
    Completed  // Пройдена (зеленый фон)
};

struct Point {
    int id;
    double x;
    double y;
    PointStatus status = PointStatus::Pending;
};
