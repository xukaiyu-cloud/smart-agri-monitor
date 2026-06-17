/*
 * sensor.h - sensor simulation layer
 */

#pragma once
#include <ctime>

struct SensorData {
    int id;
    double temp, humidity;
    int light;
    double ph;
};

SensorData sim_sensor(int point_id);
SensorData sim_sensor_at(int point_id, time_t timestamp);

// Compute 288x accelerated timestamp from a single now value.
// If reset==true, anchors the clock at now.
time_t sim_anchor(time_t now, bool reset);
