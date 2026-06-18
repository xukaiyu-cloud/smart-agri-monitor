/*
 * alert_queue.cpp - 告警链表队列实现
 * 操作：创建/销毁/入队/出队/查看/遍历
 * 技术：malloc/free、指针运算、尾插法
 */

#include "alert_queue.h"
#include <cstdlib>
#include <cstring>
using namespace std;

// ========== 创建队列 ==========
AlertQueue* aq_create(int max_size) {
    AlertQueue* q = (AlertQueue*)malloc(sizeof(AlertQueue));
    if (!q) return nullptr;
    q->front = nullptr;
    q->rear  = nullptr;
    q->size  = 0;
    q->max_size = max_size > 0 ? max_size : 100;
    return q;
}

// ========== 销毁队列 ==========
void aq_destroy(AlertQueue* q) {
    if (!q) return;
    AlertNode* cur = q->front;
    while (cur) {
        AlertNode* tmp = cur;
        cur = cur->next;
        free(tmp);  // 逐个释放结点
    }
    free(q);
}

// ========== 入队（尾插法，O(1)）==========
bool aq_enqueue(AlertQueue* q, int point_id, const char* field,
                const char* label, double value, double threshold,
                const char* level) {
    if (!q) return false;
    // 队列满时，先出队一个
    if (q->size >= q->max_size) {
        AlertNode* old = aq_dequeue(q);
        if (old) free(old);
    }

    // 动态分配新结点
    AlertNode* node = (AlertNode*)malloc(sizeof(AlertNode));
    if (!node) return false;

    node->point_id  = point_id;
    node->value     = value;
    node->threshold = threshold;
    node->timestamp = time(nullptr);
    node->next      = nullptr;

    // 字符数组安全拷贝
    strncpy(node->field, field, 15);
    node->field[15] = '\0';
    strncpy(node->label, label, 31);
    node->label[31] = '\0';
    strncpy(node->level, level, 15);
    node->level[15] = '\0';

    // 尾插法：操作 rear 指针
    if (!q->rear) {
        // 空队列：首尾指向同一结点
        q->front = node;
        q->rear  = node;
    } else {
        q->rear->next = node;  // 旧尾指向新结点
        q->rear = node;        // 更新尾指针
    }
    q->size++;
    return true;
}

// ========== 出队（头删法，O(1)）==========
AlertNode* aq_dequeue(AlertQueue* q) {
    if (!q || !q->front) return nullptr;

    AlertNode* node = q->front;   // 保存队首
    q->front = q->front->next;    // 队首后移
    if (!q->front) q->rear = nullptr;  // 队列变空
    q->size--;
    node->next = nullptr;
    return node;  // 返回结点，调用者负责 free
}

// ========== 查看队首 ==========
AlertNode* aq_peek(AlertQueue* q) {
    if (!q) return nullptr;
    return q->front;
}

// ========== 队列大小 ==========
int aq_size(AlertQueue* q) {
    if (!q) return 0;
    return q->size;
}

// ========== 判空 ==========
bool aq_is_empty(AlertQueue* q) {
    if (!q) return true;
    return q->size == 0;
}

// ========== 获取所有告警（指针数组）==========
AlertNode** aq_get_all(AlertQueue* q, int* out_count) {
    if (!q || !out_count) return nullptr;
    *out_count = q->size;
    if (q->size == 0) return nullptr;

    // 动态分配指针数组
    AlertNode** arr = (AlertNode**)malloc(q->size * sizeof(AlertNode*));
    if (!arr) return nullptr;

    AlertNode* cur = q->front;
    int i = 0;
    while (cur) {
        arr[i++] = cur;
        cur = cur->next;
    }
    return arr;
}
