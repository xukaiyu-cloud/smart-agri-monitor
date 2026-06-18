/*
 * analyze.cpp - 传感器数据分析实现
 * 算法：快速排序、指数滑动平均
 * 技术：指针数组、函数指针、动态内存
 */

#include "analyze.h"
#include "sensor.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
using namespace std;

// ========== 比较函数 ==========
int cmp_asc(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

int cmp_desc(const void* a, const void* b) {
    return -cmp_asc(a, b);
}

// ========== 快速排序（经典算法1）==========
// 使用双指针分区法
void quick_sort(double* arr, int low, int high, CompareFunc comp) {
    if (low >= high) return;

    // 三数取中法选pivot，避免最坏O(n²)
    int mid = low + (high - low) / 2;
    if (comp(&arr[low], &arr[mid]) > 0) {
        double tmp = arr[low]; arr[low] = arr[mid]; arr[mid] = tmp;
    }
    if (comp(&arr[low], &arr[high]) > 0) {
        double tmp = arr[low]; arr[low] = arr[high]; arr[high] = tmp;
    }
    if (comp(&arr[mid], &arr[high]) > 0) {
        double tmp = arr[mid]; arr[mid] = arr[high]; arr[high] = tmp;
    }
    double pivot = arr[mid];

    // 双指针分区
    int i = low, j = high;
    while (i <= j) {
        while (comp(&arr[i], &pivot) < 0) i++;
        while (comp(&arr[j], &pivot) > 0) j--;
        if (i <= j) {
            double tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            i++; j--;
        }
    }

    quick_sort(arr, low, j, comp);
    quick_sort(arr, i, high, comp);
}

// ========== 指数滑动平均（经典算法2）==========
// EMA(t) = alpha * value(t) + (1-alpha) * EMA(t-1)
double ema_filter(double* data, int count, double alpha) {
    if (count <= 0) return 0.0;
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;

    double ema = data[0];
    for (int i = 1; i < count; i++) {
        ema = alpha * data[i] + (1.0 - alpha) * ema;
    }
    return ema;
}

// ========== 二分查找（经典算法3）==========
int binary_search(double* arr, int n, double target) {
    int left = 0, right = n - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;  // 防溢出
        if (arr[mid] == target) return mid;
        if (arr[mid] < target)
            left = mid + 1;
        else
            right = mid - 1;
    }
    return -1;  // 未找到
}

int lower_bound(double* arr, int n, double threshold) {
    int left = 0, right = n;
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] < threshold)
            left = mid + 1;
        else
            right = mid;
    }
    return left;  // 返回第一个 >= threshold 的位置
}

// ========== 统计分析 ==========
SensorStats* analyze_sensor(int point_id, double* data, int count) {
    // 动态内存分配
    SensorStats* stats = (SensorStats*)malloc(sizeof(SensorStats));
    if (!stats) return nullptr;
    memset(stats, 0, sizeof(SensorStats));

    stats->point_id = point_id;
    stats->count = count;
    if (count <= 0) return stats;

    // 动态分配数据副本（用于排序）
    double* sorted = (double*)malloc(count * sizeof(double));
    if (!sorted) { free(stats); return nullptr; }
    memcpy(sorted, data, count * sizeof(double));

    // 使用快速排序 + 函数指针
    quick_sort(sorted, 0, count - 1, cmp_asc);

    // 统计量
    stats->min = sorted[0];
    stats->max = sorted[count - 1];

    // 中位数
    if (count % 2 == 1)
        stats->median = sorted[count / 2];
    else
        stats->median = (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0;

    // 均值 + 标准差
    double sum = 0.0;
    for (int i = 0; i < count; i++) sum += data[i];
    stats->mean = sum / count;

    double var = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = data[i] - stats->mean;
        var += diff * diff;
    }
    stats->stddev = sqrt(var / count);

    // 四分位数
    int q1_idx = count / 4;
    int q3_idx = (3 * count) / 4;
    stats->q1 = sorted[q1_idx];
    stats->q3 = sorted[q3_idx];

    // EMA 降噪
    stats->ema = ema_filter(data, count, 0.2);

    // 二分查找：统计超过35°C高温阈值的样本数
    int idx = lower_bound(sorted, count, 35.0);
    stats->threshold_35 = 35.0;
    stats->above_threshold = count - idx;  // 从idx到末尾都 >= 35

    free(sorted);
    return stats;
}

void free_stats(SensorStats* s) {
    free(s);
}

// 获取传感器数据并分析
SensorStats* compute_sensor_stats(int point_id) {
    // 动态分配数组（C风格）
    const int N = 144; // 取最近144个采样点（约12小时）
    double* samples = (double*)malloc(N * sizeof(double));
    if (!samples) return nullptr;

    double current_hour = sim_hour_now();
    for (int i = 0; i < N; i++) {
        double hour = current_hour - (N - 1 - i) * 5.0 / 60.0;
        // 手动处理负值
        while (hour < 0) hour += 24.0;
        while (hour >= 24.0) hour -= 24.0;
        SensorData sd = sim_sensor_at_hour(point_id, hour);
        samples[i] = sd.temp;
    }

    SensorStats* result = analyze_sensor(point_id, samples, N);
    free(samples);
    return result;
}
