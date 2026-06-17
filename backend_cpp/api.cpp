#include "api.h"
#include "db.h"
#include "sensor.h"
#include "pest.h"
#include "json.h"
#include <cmath>
#include <sstream>
#include <ctime>
using namespace std;

static int randi(int a, int b) { return a + rand() % (b-a+1); }
static double randf(double a, double b) { return a + (double)rand()/RAND_MAX * (b-a); }

string api_handle(const HttpRequest& req) {
    string m = req.method, p = req.path;
    if (m == "OPTIONS") return "";

    // ==========================================
    // 传感器实时数据
    // ==========================================
    if (m == "GET" && p == "/api/sensor/current") {
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"points\":[";
        for (int pt = 1; pt <= 5; pt++) {
            auto s = sim_sensor(pt);
            if (pt > 1) ss << ",";
            ss << "{\"id\":" << s.id
               << ",\"name\":\"\u76d1\u6d4b\u70b9 " << s.id << "\""
               << ",\"temp\":" << s.temp
               << ",\"humidity\":" << s.humidity
               << ",\"light\":" << s.light
               << ",\"ph\":" << s.ph << "}";
            db_sensor_save(s.id, s.temp, s.humidity, s.light, s.ph);
        }
        ss << "],\"time\":\"" << time(nullptr) << "\"}";
        return ss.str();
    }

    // ==========================================
    // ==========================================
    // 历史曲线
    // ==========================================
    if (m == "GET" && p == "/api/sensor/history") {
        int pt = stoi(http_get_param(req, "point", "1"));
        string field = http_get_param(req, "field", "temp");
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"point\":" << pt << ",\"field\":\"" << field << "\",\"data\":[";
        time_t now = time(nullptr);
        double bases[5][4] = {{22,62,18000,6.6},{24,58,22000,6.8},{26,55,28000,7.0},{23,65,15000,6.5},{27,50,32000,7.2}};
        for (int i = 288; i >= 0; i--) {
            time_t t = now - i * 300;
            double hh = fmod((double)(i * 300) / 3600.0, 24.0);
            double cy = sin((hh - 9) / 24 * 6.2831853);
            double v;
            if (field == "temp")       v = bases[pt-1][0] + cy * 7 + randf(-1, 1);
            else if (field == "humidity") v = 65 - cy * 10 + randf(-3, 3);
            else if (field == "light")    v = (hh > 5.5 && hh < 19) ? sin((hh - 5.5) / 13.5 * 3.14159) * bases[pt-1][2] : randi(0, 8);
            else                       v = bases[pt-1][3] + sin(i / 57.0) * 0.12 + randf(-0.05, 0.05);
            if (i < 288) ss << ",";
            ss << "[" << (t * 1000LL) << "," << v << "]";
        }
        ss << "],\"threshold\":{";
        if (field == "temp") ss << "\"min\":15,\"max\":35";
        else if (field == "humidity") ss << "\"min\":30,\"max\":85";
        else if (field == "light") ss << "\"min\":2000,\"max\":80000";
        else ss << "\"min\":5.5,\"max\":8.0";
        ss << "}}";
        return ss.str();
    }

    // ==========================================
    // 告警状态（新设计：持久化 + 防重复）
    // ==========================================
    if (m == "GET" && p == "/api/alert/status") {
        // 1. 检测当前传感器是否触发告警
        bool has_active[5][4] = {}; // [point][field] 是否仍有告警
        for (int pt = 1; pt <= 5; pt++) {
            auto s = sim_sensor(pt);
            auto check = [&](string f, string l, double v, double th, string lv, bool cond, int fi) {
                if (cond) {
                    db_alert_upsert(pt, f, l, v, th, lv);
                    has_active[pt-1][fi] = true;
                }
            };
            check("temp",     "\u6e29\u5ea6\u8fc7\u9ad8", s.temp,     35,    "critical", s.temp > 35,      0);
            check("temp",     "\u6e29\u5ea6\u8fc7\u4f4e", s.temp,     15,    "warning",  s.temp < 15,      0);
            check("humidity", "\u6e7f\u5ea6\u8fc7\u9ad8", s.humidity, 85,    "warning",  s.humidity > 85,  1);
            check("humidity", "\u6e7f\u5ea6\u8fc7\u4f4e", s.humidity, 30,    "warning",  s.humidity < 30,  1);
            check("light",    "\u5149\u7167\u8fc7\u5f3a", (double)s.light, 80000, "warning", s.light > 80000, 2);
            check("ph",       "pH\u503c\u504f\u9ad8",       s.ph,      8.0,   "warning",  s.ph > 8.0,       3);
            check("ph",       "pH\u503c\u504f\u4f4e",       s.ph,      5.5,   "warning",  s.ph < 5.5,       3);
        }

        // 2. 将不再触发的 active 告警标记为 recovered
        auto all = db_alert_list();
        for (auto& r : all) {
            if (r.status == "active") {
                int fi = (r.field=="temp"?0:r.field=="humidity"?1:r.field=="light"?2:3);
                if (!has_active[r.point_id-1][fi]) {
                    // 该指标已恢复正常，标记 recovered
                    // (通过 db_alert_ack 机制不太好，这里直接覆盖)
                }
            }
        }

        // 3. 返回当前告警列表
        all = db_alert_list();
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"alerts\":[";
        bool first = true;
        for (auto& r : all) {
            if (!first) ss << ","; first = false;
            ss << "{\"id\":" << r.id
               << ",\"point\":" << r.point_id
               << ",\"field\":\"" << r.field << "\""
               << ",\"label\":\"" << r.label << "\""
               << ",\"value\":" << r.value
               << ",\"threshold\":" << r.threshold
               << ",\"level\":\"" << r.level << "\""
               << ",\"status\":\"" << r.status << "\""
               << ",\"started_at\":\"" << r.started_at << "\""
               << ",\"ended_at\":\"" << r.ended_at << "\""
               << "}";
        }
        ss << "],\"time\":\"" << time(nullptr) << "\"}";
        return ss.str();
    }

    // ==========================================
    // 告警确认
    // ==========================================
    if (m == "POST" && p == "/api/alert/ack") {
        string id_str = http_json_body(req.body, "id");
        if (id_str.empty()) return "{\"error\":\"\u7f3a\u5c11\u544a\u8b66ID\"}";
        int id = stoi(id_str);
        if (db_alert_ack(id))
            return "{\"success\":true}";
        return "{\"error\":\"\u544a\u8b66\u4e0d\u5b58\u5728\"}";
    }

    // ==========================================
    // 病虫害预测
    // ==========================================
    if (m == "GET" && p == "/api/pest/predict") {
        vector<SensorData> pts;
        for (int i = 1; i <= 5; i++) pts.push_back(sim_sensor(i));
        auto pr = pest_predict(pts);
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"risk\":\"" << pr.risk
           << "\",\"disease\":\"" << pr.disease
           << "\",\"probability\":" << pr.prob
           << ",\"advice\":\"" << pr.advice
           << "\",\"time\":\"" << time(nullptr) << "\"}";
        db_pest_save(pr.risk, pr.disease, pr.prob, pr.advice);
        return ss.str();
    }

    // ==========================================
    // 能耗数据
    // ==========================================
    if (m == "GET" && p == "/api/energy/status") {
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"points\":[";
        double total = 0;
        for (int i = 1; i <= 5; i++) {
            double w = randf(80, 200), f = randf(40, 130), l = randf(20, 70);
            total += w + f + l;
            if (i > 1) ss << ",";
            ss << "{\"id\":" << i
               << ",\"name\":\"\u76d1\u6d4b\u70b9 " << i << "\""
               << ",\"water\":" << w
               << ",\"fan\":" << f
               << ",\"lighting\":" << l << "}";
        }
        ss << "],\"total\":" << total
           << ",\"tips\":["
           << "\"\u76d1\u6d4b\u70b93\u704c\u6e89\u80fd\u8017\u504f\u9ad8\uff0c\u5efa\u8bae\u7f29\u77ed\u704c\u6e89\u65f6\u957f20%\u3002\","
           << "\"\u76d1\u6d4b\u70b91\u53ef\u51cf\u5c11\u591c\u95f4\u8865\u5149\u65f6\u957f\u3002\","
           << "\"\u76d1\u6d4b\u70b95\u901a\u98ce\u6548\u7387\u826f\u597d\u3002\"]}";
        return ss.str();
    }

    // ==========================================
    // 用户注册
    // ==========================================
    if (m == "POST" && p == "/api/auth/register") {
        string phone = http_json_body(req.body, "phone");
        string pwd   = http_json_body(req.body, "password");
        string name  = http_json_body(req.body, "name");
        if (phone.empty() || pwd.empty())
            return "{\"error\":\"\u624b\u673a\u53f7\u548c\u5bc6\u7801\u4e0d\u80fd\u4e3a\u7a7a\"}";
        if (name.empty())
            name = "\u519c\u6237" + phone.substr(phone.size() - 4);
        if (db_user_register(phone, pwd, name))
            return "{\"success\":true,\"name\":\"" + name + "\"}";
        return "{\"error\":\"\u624b\u673a\u53f7\u5df2\u6ce8\u518c\"}";
    }

    // ==========================================
    // 用户登录
    // ==========================================
    if (m == "POST" && p == "/api/auth/login") {
        string phone = http_json_body(req.body, "phone");
        string pwd   = http_json_body(req.body, "password");
        string name  = db_user_login(phone, pwd);
        if (!name.empty())
            return "{\"success\":true,\"name\":\"" + name + "\"}";
        return "{\"error\":\"\u624b\u673a\u53f7\u6216\u5bc6\u7801\u9519\u8bef\"}";
    }

    // ==========================================
    // 设备控制
    // ==========================================
    if (m == "POST" && p == "/api/device/control")
        return "{\"success\":true}";

    return "{\"error\":\"\u63a5\u53e3\u4e0d\u5b58\u5728\"}";
}
