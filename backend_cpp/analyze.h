/*
 * analyze.h - 传感器数据分析模块
 * 包含：快速排序、指数滑动平均、统计量计算
 * 展示：指针数组、函数指针、动态内存分配
 */

#pragma once
#include "sensor.h"

// 统计结果结构体
struct SensorStats {
    int point_id;
    double min, max;
    double median, mean, stddev;
    double q1, q3;        // 第一、第三四分位数
    double ema;            // 指数滑动平均值
    int count;             // 样本数
    int above_threshold;   // 超过阈值的样本数（二分查找）
    double threshold_35;   // 35°C 高温阈值
};

// 函数指针类型：比较函数
typedef int (*CompareFunc)(const void* a, const void* b);

// ========== 经典算法1：快速排序 ==========
// arr: 待排序数组指针, low/high: 左右边界, comp: 比较函数指针
void quick_sort(double* arr, int low, int high, CompareFunc comp);

// 比较函数（供函数指针使用）
int cmp_asc(const void* a, const void* b);
int cmp_desc(const void* a, const void* b);

// ========== 经典算法2：指数滑动平均(EMA) ==========
// 用于传感器数据降噪
double ema_filter(double* data, int count, double alpha);

// ========== 经典算法3：二分查找 ==========
// 在已排序数组中查找目标值，返回下标；未找到返回 -1
// arr: 排序后的数组指针, n: 数组长度, target: 目标值
int binary_search(double* arr, int n, double target);

// 在已排序数组中找第一个 >= threshold 的位置（下界）
int lower_bound(double* arr, int n, double threshold);

// ========== 统计分析（综合运用）==========
// 使用指针数组管理排序后的数据，函数指针指定排序策略
// 返回 malloc 分配的堆内存，调用者负责 free_stats()
SensorStats* analyze_sensor(int point_id, double* data, int count);

// 释放统计结果
void free_stats(SensorStats* s);

// 计算并返回统计结果（从传感器获取数据）
SensorStats* compute_sensor_stats(int point_id);
