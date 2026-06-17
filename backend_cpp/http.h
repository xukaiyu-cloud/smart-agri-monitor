/*
 * http.h - HTTP 请求解析
 * 用途：解析原始 HTTP 报文，提取 method/path/params/body
 */

#pragma once
#include <string>
#include <map>
#include <vector>
using namespace std;

struct HttpRequest {
    string method;
    string path;
    string body;
    map<string,string> headers;
    map<string,string> params;   // URL 查询参数
};

HttpRequest http_parse(const string& raw);
string http_get_param(const HttpRequest& req, const string& key, const string& def="");
string http_json_body(const string& body, const string& key);
