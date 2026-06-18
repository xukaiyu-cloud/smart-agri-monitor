#include "api.h"
#include "db.h"
#include "sensor.h"
#include "pest.h"
#include "analyze.h"
#include "alert_queue.h"
#include "json.h"
#include <cmath>
#include <sstream>
#include <ctime>
using namespace std;

// 全局告警队列（单链表FIFO，最大100条）
static AlertQueue* g_alert_queue = nullptr;

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
    // 历史曲线
    // ==========================================
    if (m == "GET" && p == "/api/sensor/history") {
        int pt = stoi(http_get_param(req, "point", "1"));
        fprintf(stderr, "[HISTORY] full_path=%s\n", req.path.c_str());
        for (auto& kv : req.params) fprintf(stderr, "[HISTORY] param: %s=%s\n", kv.first.c_str(), kv.second.c_str());
        string field = http_get_param(req, "field", "temp");
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"point\":" << pt << ",\"field\":\"" << field << "\",\"data\":[";
        time_t now = time(nullptr);
        time_t sim_now = sim_anchor(now, http_get_param(req, "reset", "") == "1");
        fprintf(stderr, "[HISTORY] reset=%s now=%lld sim_now=%lld diff=%lld\n", http_get_param(req, "reset", "").c_str(), (long long)now, (long long)sim_now, (long long)(sim_now-now));
        double raw[289];
        time_t timestamps[289];
                for (int i = 0; i <= 288; i++) {
            time_t sim_ts = sim_now - (time_t)((288 - i) * 300);
            time_t ts = (time_t)sim_ts;
            struct tm* lt = localtime(&ts);
            double vhour = lt->tm_hour + lt->tm_min / 60.0 + lt->tm_sec / 3600.0;
            auto sd = sim_sensor_at_hour(pt, vhour);
            if (field == "temp")       raw[i] = sd.temp;
            else if (field == "humidity") raw[i] = sd.humidity;
            else if (field == "light")    raw[i] = (double)sd.light;
            else                       raw[i] = sd.ph;
            timestamps[i] = sim_ts;
        }
        for (int i = 0; i <= 288; i++) {
            double v = raw[i];
            if (i > 0 && i < 288)
                v = (raw[i-1] + raw[i] + raw[i+1]) / 3.0;
            if (i > 0) ss << ",";
            ss << "[" << (timestamps[i] * 1000LL) << "," << v << "]";
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
    // 告警实时状态接口
    // ==========================================
    if (m == "GET" && p == "/api/alert/status") {
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"alerts\":[";
        bool first = true;
        for (int pt = 1; pt <= 5; pt++) {
            auto s = sim_sensor(pt);
            auto emit = [&](const char* f, const char* l, double v, double th, const char* lv) {
                if (!first) ss << ","; first = false;
                ss << "{\"point\":" << pt
                   << ",\"field\":\"" << f << "\""
                   << ",\"label\":\"" << l << "\""
                   << ",\"value\":" << v
                   << ",\"threshold\":" << th
                   << ",\"level\":\"" << lv << "\"}";
            };
            if (s.temp > 35)      { emit("temp","温度过高", s.temp, 35, "critical");
                if (!g_alert_queue) g_alert_queue = aq_create(100);
                aq_enqueue(g_alert_queue, pt, "temp", "温度过高", s.temp, 35, "critical"); }
            if (s.temp < 15)      { emit("temp","温度过低", s.temp, 15, "warning");
                if (!g_alert_queue) g_alert_queue = aq_create(100);
                aq_enqueue(g_alert_queue, pt, "temp", "温度过低", s.temp, 15, "warning"); }
            if (s.humidity > 85)  { emit("humidity","湿度过高", s.humidity, 85, "warning");
                aq_enqueue(g_alert_queue, pt, "humidity", "湿度过高", s.humidity, 85, "warning"); }
            if (s.humidity < 30)  { emit("humidity","湿度过低", s.humidity, 30, "warning");
                aq_enqueue(g_alert_queue, pt, "humidity", "湿度过低", s.humidity, 30, "warning"); }
            if (s.light > 80000)  { emit("light","光照过强", (double)s.light, 80000, "warning");
                aq_enqueue(g_alert_queue, pt, "light", "光照过强", (double)s.light, 80000, "warning"); }
            if (s.ph > 8.0)       { emit("ph","pH值偏高", s.ph, 8.0, "warning");
                aq_enqueue(g_alert_queue, pt, "ph", "pH值偏高", s.ph, 8.0, "warning"); }
            if (s.ph < 5.5)       { emit("ph","pH值偏低", s.ph, 5.5, "warning");
                aq_enqueue(g_alert_queue, pt, "ph", "pH值偏低", s.ph, 5.5, "warning"); }
        }
        ss << "]}";
        return ss.str();
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
    // 告警队列查看（链表遍历）
    // ==========================================
    if (m == "GET" && p == "/api/alert/queue") {
        if (!g_alert_queue) g_alert_queue = aq_create(100);
        stringstream ss; ss.precision(1); ss << fixed;
        ss << "{\"queue\":[";
        int count = 0;
        AlertNode** arr = aq_get_all(g_alert_queue, &count);
        if (arr) {
            for (int i = 0; i < count; i++) {
                if (i > 0) ss << ",";
                ss << "{\"point\":" << arr[i]->point_id
                   << ",\"field\":\"" << arr[i]->field << "\""
                   << ",\"label\":\"" << arr[i]->label << "\""
                   << ",\"value\":" << arr[i]->value
                   << ",\"threshold\":" << arr[i]->threshold
                   << ",\"level\":\"" << arr[i]->level << "\""
                   << ",\"timestamp\":" << arr[i]->timestamp << "}";
            }
            free(arr);
        }
        ss << "],\"size\":" << aq_size(g_alert_queue) << "}";
        return ss.str();
    }

    // ==========================================
    // 传感器统计分析（快速排序 + EMA）
    // ==========================================
    if (m == "GET" && p == "/api/sensor/stats") {
        int pt = stoi(http_get_param(req, "point", "1"));
        SensorStats* s = compute_sensor_stats(pt);
        stringstream ss; ss.precision(1); ss << fixed;
        if (s) {
            ss << "{\"point\":" << s->point_id
               << ",\"count\":" << s->count
               << ",\"min\":" << s->min
               << ",\"max\":" << s->max
               << ",\"median\":" << s->median
               << ",\"mean\":" << s->mean
               << ",\"stddev\":" << s->stddev
               << ",\"q1\":" << s->q1
               << ",\"q3\":" << s->q3
               << ",\"ema\":" << s->ema
               << ",\"threshold_35\":" << s->threshold_35
               << ",\"above_threshold\":" << s->above_threshold << "}";
            free_stats(s);
        } else {
            ss << "{\"error\":\"stat_failed\"}";
        }
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
            double w = randf(80, 200), f = randf(40, 130), l = 0;
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
    // 灌溉/通风建议
    // ==========================================
    if (m == "GET" && p == "/api/sensor/advice") {
        int pt = stoi(http_get_param(req, "point", "1"));
        string device = http_get_param(req, "device", "water");
        auto s = sim_sensor(pt);
        stringstream ss; ss.precision(1); ss << fixed;
        bool advisable = true;
        string reason;
        if (device == "water") {
            if (s.humidity > 60) {
                advisable = false;
                ss << "当前湿度" << s.humidity << "%，土壤水分充足，不建议灌溉";
                reason = ss.str();
            }
        } else if (device == "fan") {
            if (s.temp < 30 && s.humidity < 70) {
                advisable = false;
                ss << "当前温度" << s.temp << "°C、湿度" << s.humidity << "%，环境良好，不建议通风";
                reason = ss.str();
            }
        }
        stringstream resp; resp.precision(1); resp << fixed;
        resp << "{\"advisable\":" << (advisable ? "true" : "false")
             << ",\"reason\":\"" << reason << "\""
             << ",\"field\":\"" << (device == "water" ? "humidity" : "temp") << "\""
             << ",\"current\":" << (device == "water" ? s.humidity : s.temp)
             << ",\"threshold\":" << (device == "water" ? 60 : 30) << "}";
        return resp.str();
    }

    // ==========================================
    // 检查设备是否应该关闭
    // ==========================================
    if (m == "GET" && p == "/api/sensor/should_stop") {
        int pt = stoi(http_get_param(req, "point", "1"));
        string device = http_get_param(req, "device", "water");
        auto s = sim_sensor(pt);
        stringstream ss; ss.precision(1); ss << fixed;
        bool should_stop = false;
        string reason;
        if (device == "water") {
            should_stop = (s.humidity > 60);
            if (should_stop) {
                ss << "湿度已回升至" << s.humidity << "%，可停止灌溉";
                reason = ss.str();
            }
        } else if (device == "fan") {
            should_stop = (s.temp < 30 && s.humidity < 70);
            if (should_stop) {
                ss << "温度已降至" << s.temp << "°C，环境恢复正常，可停止通风";
                reason = ss.str();
            }
        }
        stringstream resp; resp.precision(1); resp << fixed;
        resp << "{\"should_stop\":" << (should_stop ? "true" : "false")
             << ",\"reason\":\"" << reason << "\"}";
        return resp.str();
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
