/*
 * api.h - API 路由控制层
 * 用途：将 HTTP 请求分发到对应的业务处理函数
 */

#pragma once
#include <string>
#include "http.h"
using namespace std;

string api_handle(const HttpRequest& req);
