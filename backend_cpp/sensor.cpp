/*
 * sensor.cpp - 传感器模拟（288倍加速，5分钟=24小时）
 */

#include "sensor.h"
#include <cmath>
#include <random>
#include <ctime>
using namespace std;

static random_device rd;
static mt19937 rng(rd());
static uniform_real_distribution<double> uni(0.0,1.0);

static double gauss(double m, double s) {
    double u = uni(rng), v = uni(rng);
    return m + s*sqrt(-2*log(u))*cos(6.2831853*v);
}

// 288倍加速，启动时对齐当前北京时间
static time_t g_start = 0;
static double sim_hour_now() {
    if (g_start == 0) g_start = time(nullptr);
    time_t el = time(nullptr) - g_start;
    return fmod(el * 300.0 / 3600.0, 24.0);
}
static double sim_hour_at(time_t ts) {
    if (g_start == 0) g_start = time(nullptr);
    time_t el = ts - g_start;
    return fmod(el * 300.0 / 3600.0, 24.0);
}

// 共享的传感器逻辑
static SensorData make_sensor(int pt, double hour) {
    double bases[5][4] = {
        {22,62,18000,6.6}, {24,58,22000,6.8}, {26,55,28000,7.0},
        {23,65,15000,6.5}, {27,50,32000,7.2}
    };
    auto& b = bases[pt-1];
    double day = sin((hour-9)/24*6.2831853);
    double temp = b[0] + day*7 + gauss(0,0.3);
    double hum  = 65 - (temp-b[0])*3 + gauss(0,1.2);
    int light;
    if(hour>5.5 && hour<19) {
        double sun = sin((hour-5.5)/13.5*3.14159);
        light = (int)(sun*b[2] + gauss(0,b[2]*0.08));
        if(uni(rng)<0.15) light = (int)(light*(0.65+uni(rng)*0.25));
    } else light = (int)(uni(rng)*8);
    double ph = b[3] + gauss(0,0.04);
    return {pt, max(0.0,temp), max(0.0,min(100.0,hum)), max(0,light), max(0.0,min(14.0,ph))};
}

time_t sim_anchor(time_t now, bool reset) {
    if (reset || g_start == 0) g_start = now;
    return g_start + (now - g_start) * 300;
}

SensorData sim_sensor(int pt) {
    return make_sensor(pt, sim_hour_now());
}

SensorData sim_sensor_at(int pt, time_t ts) {
    return make_sensor(pt, sim_hour_at(ts));
}