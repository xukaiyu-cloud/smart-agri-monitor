#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include "http.h"
#include "api.h"
#include "db.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

void handle_client(SOCKET client) {
    try {
    char buf[8192];
    int n = recv(client, buf, sizeof(buf)-1, 0);
    if(n<=0) { closesocket(client); return; }
    buf[n]=0;
    string raw(buf);
    if (raw.find("Expect: 100-continue") != string::npos) {
        string cont = "HTTP/1.1 100 Continue\r\n\r\n";
        send(client, cont.c_str(), (int)cont.size(), 0);
        int cl = 0;
        auto cl_pos = raw.find("Content-Length: ");
        if (cl_pos != string::npos) cl = stoi(raw.substr(cl_pos + 16));
        if (cl > 0 && cl < 8192) {
            int body_n = recv(client, buf, cl, 0);
            if (body_n > 0) { buf[body_n] = 0; raw += string(buf); }
        }
    }
    auto req = http_parse(raw);
    string body = api_handle(req);
    string status = body.find("\"error\"") != string::npos ? "400 Bad Request" : "200 OK";
    stringstream resp;
    resp << "HTTP/1.1 " << status << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp << "Access-Control-Allow-Headers: Content-Type\r\n";
    if (!body.empty()) {
        resp << "Content-Type: application/json; charset=utf-8\r\n";
        resp << "Content-Length: " << body.size() << "\r\n";
    }
    resp << "Connection: close\r\n\r\n" << body;
    string rs = resp.str();
    send(client, rs.c_str(), (int)rs.size(), 0);
    closesocket(client);
    } catch (...) { closesocket(client); }
}

int main() {
    db_init();
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr={};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(server, (sockaddr*)&addr, sizeof(addr));
    listen(server, 10);
    cout << "================================================" << endl;
    cout << "  智慧农业监控系统 C++ 后端" << endl;
    cout << "  http://localhost:8080" << endl;
    cout << "================================================" << endl;
    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        thread(handle_client, client).detach();
    }
}
