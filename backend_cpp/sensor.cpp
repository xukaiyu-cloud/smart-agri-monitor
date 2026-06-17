/*
 * sensor.cpp - 传感器模拟
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

SensorData sim_sensor(int pt) {
    time_t now = time(nullptr); tm* t = localtime(&now);
    double hour = t->tm_hour + t->tm_min/60.0;

    // 5个监测点基线 (温度,湿度,光照,pH)
    double bases[5][4] = {
        {22,62,18000,6.6}, {24,58,22000,6.8}, {26,55,28000,7.0},
        {23,65,15000,6.5}, {27,50,32000,7.2}
    };
    auto& b = bases[pt-1];
    double day = sin((hour-9)/24*6.2831853);

    double temp = b[0] + day*7 + gauss(0,0.8);
    double hum  = 65 - (temp-b[0])*3 + gauss(0,3);
    int light;
    if(hour>5.5 && hour<19) {
        double sun = sin((hour-5.5)/13.5*3.14159);
        light = (int)(sun*b[2] + gauss(0,b[2]*0.08));
        if(uni(rng)<0.2) light = (int)(light*(0.3+uni(rng)*0.5));
    } else light = (int)(uni(rng)*8);
    double ph = b[3] + gauss(0,0.04);

    return {pt, max(0.0,temp), max(0.0,min(100.0,hum)), max(0,light), max(0.0,min(14.0,ph))};
}