/*
 * db.h - SQLite 数据库层
 */

#pragma once
#include <string>
#include <vector>
using namespace std;

struct AlertRow {
    int id, point_id;
    string field, label, level, status;
    double value, threshold;
    string started_at, ended_at;
};

void db_init();
string db_hash_pwd(const string& pwd);
bool db_user_register(const string& phone, const string& pwd, const string& name);
string db_user_login(const string& phone, const string& pwd);
void db_sensor_save(int pt, double temp, double hum, int light, double ph);

// 告警（新设计）
bool db_alert_upsert(int pt, const string& field, const string& label,
                     double value, double th, const string& level);
vector<AlertRow> db_alert_list();
bool db_alert_ack(int alert_id);
void db_alert_resolve_others(int pt, const string& field);

void db_pest_save(const string& risk, const string& disease, double prob, const string& advice);
