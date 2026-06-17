/*
 * pest.h - 病虫害预警模型
 * 用途：基于传感器数据，用规则引擎判断病害风险
 */

#pragma once
#include <string>
#include <vector>
#include "sensor.h"
using namespace std;

struct PestResult {
    string risk;      // low / medium / high
    string disease;   // 病害名称
    double prob;      // 概率%
    string advice;    // 建议措施
};

PestResult pest_predict(const vector<SensorData>& points);
