/*
 * http.cpp - HTTP 请求解析实现
 */

#include "http.h"
#include <sstream>
#include <cstdio>
using namespace std;

static string trim(const string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    auto e = s.find_last_not_of(" \t\r\n");
    return (b==string::npos)?"":s.substr(b,e-b+1);
}

static vector<string> split(const string& s, char delim) {
    vector<string> r; stringstream ss(s); string item;
    while(getline(ss,item,delim)) r.push_back(item);
    return r;
}

static string url_decode(const string& s) {
    string r;
    for(size_t i=0;i<s.size();i++) {
        if(s[i]=='%' && i+2<s.size()) {
            int v; sscanf(s.substr(i+1,2).c_str(),"%x",&v);
            r+=(char)v; i+=2;
        } else if(s[i]=='+') r+=' ';
        else r+=s[i];
    }
    return r;
}

HttpRequest http_parse(const string& raw) {
    HttpRequest req;
    auto lines = split(raw,'\n');
    if(lines.empty()) return req;

    auto parts = split(trim(lines[0]),' ');
    if(parts.size()>=2) { req.method=parts[0]; req.path=parts[1]; }

    auto q = req.path.find('?');
    if(q!=string::npos) {
        string qs = req.path.substr(q+1);
        req.path = req.path.substr(0,q);
        for(auto& p:split(qs,'&')) {
            auto eq = p.find('=');
            if(eq!=string::npos)
                req.params[url_decode(p.substr(0,eq))]=url_decode(p.substr(eq+1));
        }
    }

    size_t i=1;
    for(;i<lines.size()&&!trim(lines[i]).empty();i++) {
        auto col = lines[i].find(':');
        if(col!=string::npos)
            req.headers[trim(lines[i].substr(0,col))]=trim(lines[i].substr(col+1));
    }

    for(i++;i<lines.size();i++)
        req.body+=lines[i]+(i<lines.size()-1?"\n":"");
    req.body = trim(req.body);
    return req;
}

string http_get_param(const HttpRequest& req, const string& key, const string& def) {
    auto it = req.params.find(key);
    return it!=req.params.end()?it->second:def;
}

string http_json_body(const string& body, const string& key) {
    string k = "\""+key+"\"";
    auto p = body.find(k); if(p==string::npos) return "";
    p = body.find(':',p); if(p==string::npos) return "";
    p = body.find('"',p); if(p==string::npos) return "";
    auto e = body.find('"',p+1); if(e==string::npos) return "";
    return body.substr(p+1,e-p-1);
}
