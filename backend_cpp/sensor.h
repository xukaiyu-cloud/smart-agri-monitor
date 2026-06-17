/*
 * sensor.h - 传感器模拟层
 * 用途：模拟5个监测点的温湿度、光照、pH数据
 */

#pragma once

struct SensorData {
    int id;
    double temp, humidity;
    int light;
    double ph;
};

// 模拟单个监测点数据
SensorData sim_sensor(int point_id);