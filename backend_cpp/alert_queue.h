/*
 * alert_queue.h - 告警链表队列（FIFO）
 * 数据结构：单链表 + 队列指针
 * 展示：指针链表、动态内存、结构体嵌套
 */

#pragma once
#include <ctime>

// 告警结点（链表节点）
typedef struct AlertNode {
    int   point_id;
    char  field[16];        // temp/humidity/light/ph
    char  label[32];        // 告警描述
    double value;           // 当前值
    double threshold;       // 阈值
    char  level[16];        // critical/warning
    time_t timestamp;       // 发生时间
    struct AlertNode* next; // 指向下一结点（指针）
} AlertNode;

// 告警队列（FIFO）
typedef struct {
    AlertNode* front;   // 队首指针
    AlertNode* rear;    // 队尾指针
    int size;           // 当前元素数
    int max_size;       // 最大容量
} AlertQueue;

// 创建队列（malloc 动态分配）
AlertQueue* aq_create(int max_size);

// 销毁队列（free 所有结点）
void aq_destroy(AlertQueue* q);

// 入队（尾插法，O(1)）
bool aq_enqueue(AlertQueue* q, int point_id, const char* field,
                const char* label, double value, double threshold,
                const char* level);

// 出队（头删法，O(1)），返回出队结点（需调用者 free）
AlertNode* aq_dequeue(AlertQueue* q);

// 查看队首（不移除）
AlertNode* aq_peek(AlertQueue* q);

// 队列大小
int aq_size(AlertQueue* q);

// 判空
bool aq_is_empty(AlertQueue* q);

// 获取所有告警（用于 JSON 输出），返回结点指针数组
AlertNode** aq_get_all(AlertQueue* q, int* out_count);
