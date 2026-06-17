/*
 * json.h - JSON 构建工具
 * 用途：将 C++ 数据组装为 JSON 字符串，返回给前端
 */

#pragma once
#include <string>
#include <vector>
using namespace std;

// 字符串转 JSON 字符串（处理转义）
inline string json_str(const string& s) {
    string r = "\"";
    for (char c : s) { if (c=='"'||c=='\\') r+='\\'; r+=c; }
    return r + "\"";
}

// 键值对 → JSON 对象
inline string json_obj(const vector<pair<string,string>>& fields) {
    string r = "{";
    for (size_t i=0; i<fields.size(); i++) {
        if (i) r += ",";
        r += json_str(fields[i].first) + ":" + fields[i].second;
    }
    return r + "}";
}

// 数组 → JSON 数组
inline string json_arr(const vector<string>& items) {
    string r = "[";
    for (size_t i=0; i<items.size(); i++) { if(i) r+=","; r+=items[i]; }
    return r + "]";
}
