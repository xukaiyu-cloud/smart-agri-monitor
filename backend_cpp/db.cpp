/*
 * db.cpp - SQLite 数据库层实现（线程安全）
 */

#include "db.h"
#include "sqlite3.h"
#include <mutex>
#include <cstdio>
using namespace std;

static sqlite3* g_db = nullptr;
static mutex g_db_mutex;

void db_init() {
    lock_guard<mutex> lk(g_db_mutex);
    sqlite3_open("D:/Wpro/backend_cpp/agri.db", &g_db);
    if (!g_db) return;
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL", 0, 0, 0);

    // 开发阶段直接删旧表重建
    sqlite3_exec(g_db, "DROP TABLE IF EXISTS alerts", 0, 0, 0);

    const char* sql =
        "CREATE TABLE IF NOT EXISTS users("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  phone TEXT UNIQUE NOT NULL,"
        "  password TEXT NOT NULL,"
        "  name TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT (datetime('now','localtime'))"
        ");"
        "CREATE TABLE IF NOT EXISTS sensors("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  point_id INTEGER NOT NULL,"
        "  temp REAL, humidity REAL, light INTEGER, ph REAL,"
        "  timestamp DATETIME DEFAULT (datetime('now','localtime'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sensor_pt_time ON sensors(point_id, timestamp);"
        "CREATE TABLE IF NOT EXISTS alerts("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  point_id INTEGER NOT NULL,"
        "  field TEXT NOT NULL,"
        "  label TEXT NOT NULL,"
        "  value REAL,"
        "  threshold REAL,"
        "  level TEXT NOT NULL,"
        "  status TEXT DEFAULT 'active',"
        "  started_at DATETIME DEFAULT (datetime('now','localtime')),"
        "  ended_at DATETIME,"
        "  ack_at DATETIME"
        ");"
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_alert_active_uniq "
        "  ON alerts(point_id, field) WHERE status = 'active';"
        "CREATE TABLE IF NOT EXISTS pest("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  risk TEXT, disease TEXT, probability REAL, advice TEXT,"
        "  timestamp DATETIME DEFAULT (datetime('now','localtime'))"
        ");";
    char* err = nullptr;
    int rc = sqlite3_exec(g_db, sql, 0, 0, &err);
    if (rc != SQLITE_OK && err) {
        fprintf(stderr, "[DB] init error: %s\n", err);
        sqlite3_free(err);
    }
}

string db_hash_pwd(const string& pwd) {
    unsigned long h = 5381;
    for (char c : pwd) h = ((h << 5) + h) + c;
    return to_string(h);
}

bool db_user_register(const string& phone, const string& pwd, const string& name) {
    lock_guard<mutex> lk(g_db_mutex);
    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT OR IGNORE INTO users(phone,password,name) VALUES('%s','%s','%s')",
        phone.c_str(), db_hash_pwd(pwd).c_str(), name.c_str());
    int rc = sqlite3_exec(g_db, sql, 0, 0, 0);
    if (rc != SQLITE_OK) return false;
    return sqlite3_changes(g_db) > 0;
}

string db_user_login(const string& phone, const string& pwd) {
    lock_guard<mutex> lk(g_db_mutex);
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT name FROM users WHERE phone='%s' AND password='%s'",
        phone.c_str(), db_hash_pwd(pwd).c_str());
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, 0) != SQLITE_OK) return "";
    string name;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        name = (const char*)sqlite3_column_text(stmt, 0);
    sqlite3_finalize(stmt);
    return name;
}

void db_sensor_save(int pt, double temp, double hum, int light, double ph) {
    lock_guard<mutex> lk(g_db_mutex);
    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT INTO sensors(point_id,temp,humidity,light,ph) VALUES(%d,%g,%g,%d,%g)",
        pt, temp, hum, light, ph);
    sqlite3_exec(g_db, sql, 0, 0, 0);
}

// ===== 告警系统（新设计）=====

bool db_alert_upsert(int pt, const string& field, const string& label,
                     double value, double th, const string& level) {
    lock_guard<mutex> lk(g_db_mutex);
    // 先检查是否已有 active 的同指标告警
    char check_sql[256];
    snprintf(check_sql, sizeof(check_sql),
        "SELECT id FROM alerts WHERE point_id=%d AND field='%s' AND status='active'",
        pt, field.c_str());
    sqlite3_stmt* stmt = nullptr;
    bool exists = false;
    if (sqlite3_prepare_v2(g_db, check_sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) exists = true;
        sqlite3_finalize(stmt);
    }
    if (exists) {
        // 更新 value，不重置时间
        char upd[256];
        snprintf(upd, sizeof(upd),
            "UPDATE alerts SET value=%g WHERE point_id=%d AND field='%s' AND status='active'",
            value, pt, field.c_str());
        sqlite3_exec(g_db, upd, 0, 0, 0);
        return true;
    }
    // 新建告警
    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO alerts(point_id,field,label,value,threshold,level,status,started_at) "
        "VALUES(%d,'%s','%s',%g,%g,'%s','active',datetime('now','localtime'))",
        pt, field.c_str(), label.c_str(), value, th, level.c_str());
    int rc = sqlite3_exec(g_db, sql, 0, 0, 0);
    return rc == SQLITE_OK;
}

void db_alert_resolve_others(int pt, const string& field) {
    // 当指标恢复正常时，标记同监测点其他 active 告警为 recovered
    // 这个由外部逻辑调用，这里只做 UPDATE
    lock_guard<mutex> lk(g_db_mutex);
    char sql[256];
    // 不在此处自动 resolve，由 api 层决定
    (void)pt; (void)field;
}

vector<AlertRow> db_alert_list() {
    lock_guard<mutex> lk(g_db_mutex);
    vector<AlertRow> result;
    const char* sql =
        "SELECT id,point_id,field,label,value,threshold,level,status,"
        "  COALESCE(started_at,''), COALESCE(ended_at,'') "
        "FROM alerts ORDER BY "
        "  CASE status WHEN 'active' THEN 0 WHEN 'acked' THEN 1 ELSE 2 END, "
        "  started_at DESC LIMIT 50";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, 0) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AlertRow r;
        r.id         = sqlite3_column_int(stmt, 0);
        r.point_id   = sqlite3_column_int(stmt, 1);
        r.field      = (const char*)sqlite3_column_text(stmt, 2);
        r.label      = (const char*)sqlite3_column_text(stmt, 3);
        r.value      = sqlite3_column_double(stmt, 4);
        r.threshold  = sqlite3_column_double(stmt, 5);
        r.level      = (const char*)sqlite3_column_text(stmt, 6);
        r.status     = (const char*)sqlite3_column_text(stmt, 7);
        r.started_at = (const char*)sqlite3_column_text(stmt, 8);
        r.ended_at   = (const char*)sqlite3_column_text(stmt, 9);
        result.push_back(r);
    }
    sqlite3_finalize(stmt);
    return result;
}

bool db_alert_ack(int alert_id) {
    lock_guard<mutex> lk(g_db_mutex);
    char sql[128];
    snprintf(sql, sizeof(sql),
        "UPDATE alerts SET status='acked', ack_at=datetime('now','localtime') WHERE id=%d",
        alert_id);
    int rc = sqlite3_exec(g_db, sql, 0, 0, 0);
    return rc == SQLITE_OK && sqlite3_changes(g_db) > 0;
}

void db_sensor_save_already_locked(int pt, double temp, double hum, int light, double ph);

void db_pest_save(const string& risk, const string& disease,
                  double prob, const string& advice) {
    lock_guard<mutex> lk(g_db_mutex);
    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO pest(risk,disease,probability,advice) VALUES('%s','%s',%g,'%s')",
        risk.c_str(), disease.c_str(), prob, advice.c_str());
    sqlite3_exec(g_db, sql, 0, 0, 0);
}
