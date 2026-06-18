# 智慧农业监控系统

> 《数据结构与算法》课程设计第4题 | v2.0

## 项目简介

设计农业环境监测系统，接收多源传感器数据，分析温湿度、光照、土壤pH值，自动触发告警并生成灌溉/通风决策。

## 技术架构

| 层级 | 技术 | 说明 |
|------|------|------|
| 后端 | C++ (C++11) | HTTP服务器 + 传感器模拟 + SQLite持久化 |
| 前端 | HTML/CSS/JS | ECharts可视化 + 响应式布局 |
| 移动端 | Capacitor 6 | Android APK 打包 |
| 数据库 | SQLite 3 | 用户/传感器/告警/病虫害 4表 |

## 项目结构

`
D:\Wpro\
├── backend_cpp/           # C++ 后端（端口8080）
│   ├── main.cpp            HTTP服务器（多线程）
│   ├── api.cpp             路由分发（11个API接口）
│   ├── sensor.cpp/h        传感器模拟（300倍加速）
│   ├── db.cpp/h            SQLite 数据持久化
│   ├── pest.cpp/h          病虫害规则引擎
│   ├── http.cpp/h          HTTP 协议解析
│   ├── analyze.cpp/h       统计分析（快速排序+EMA+二分查找）
│   ├── alert_queue.cpp/h   告警链表队列（FIFO）
│   ├── json.h / sqlite3.h  第三方头文件
│   ├── server.exe          编译后可执行文件
│   └── run.bat             启动脚本
├── smart_agri_app/         # 前端源码
│   ├── index.html          主页面（登录+仪表盘）
│   ├── css/style.css       样式表
│   ├── js/
│   │   ├── app.js          主逻辑（轮询/渲染/弹框/时间）
│   │   ├── api.js          HTTP 请求层
│   │   ├── chart.js        ECharts 图表管理
│   │   └── native.js       Capacitor 原生桥接
│   ├── assets/             图标资源
│   ├── www/                Capacitor webDir（打包用）
│   ├── android/            Android 平台
│   ├── server.py           Python 开发服务器（端口8081）
│   └── package.json        Capacitor 依赖
└── task_book.docx         课程设计任务书
`

## 快速启动

### 1. 启动后端（C++）

`cmd
cd /d D:\Wpro\backend_cpp
server.exe
`

### 2. 启动前端（Python）

`cmd
cd /d D:\Wpro\smart_agri_app
python server.py
`

### 3. 打开浏览器

`
http://localhost:8081
`

默认账号：手机号 13800000001 密码 123456

## API 接口 (11个)

| 路由 | 方法 | 用途 |
|------|------|------|
| /api/sensor/current | GET | 5监测点实时数据 |
| /api/sensor/history | GET | 289点历史曲线 |
| /api/sensor/advice | GET | 灌溉/通风建议 |
| /api/sensor/should_stop | GET | 是否应关闭设备 |
| /api/sensor/stats | GET | 统计分析 |
| /api/alert/status | GET | 当前告警列表 |
| /api/alert/queue | GET | 告警链表队列 |
| /api/pest/predict | GET | 病虫害预测 |
| /api/energy/status | GET | 能耗数据 |
| /api/device/control | POST | 设备开关控制 |
| /api/auth/register | POST | 用户注册 |
| /api/auth/login | POST | 用户登录 |

## 数据结构与算法

| 技术要求 | 实现 | 位置 |
|----------|------|------|
| 快速排序（三数取中+双指针分区） | ✅ | nalyze.cpp |
| 指数滑动平均 EMA | ✅ | nalyze.cpp |
| 二分查找（含 lower_bound） | ✅ | nalyze.cpp |
| 单链表FIFO队列（尾插法+头删法） | ✅ | lert_queue.cpp |
| 函数指针（CompareFunc） | ✅ | nalyze.h |
| 指针数组（AlertNode**） | ✅ | lert_queue.cpp |
| 动态内存（malloc/free） | ✅ | 多处使用 |
| 结构体/字符数组/字符串操作 | ✅ | 全部模块 |
| SQLite 持久化存储 | ✅ | db.cpp |

## 课设要求覆盖

- ✅ 接收多源传感器数据（温湿度、光照、土壤pH值）
- ✅ 设定环境阈值自动触发警报
- ✅ 历史数据曲线可视化（ECharts）
- ✅ 设备控制指令生成（灌溉、通风开关）
- ✅ 病虫害预警模型
- ✅ 移动端监控（Capacitor Android）
- ✅ 能耗优化建议
- ✅ 2种以上经典算法
- ✅ 指针、结构体、链表等C/C++技术

## 编译后端

`cmd
g++ -o D:\Wpro\server_new.exe ^
  D:\Wpro\backend_cpp\main.cpp ^
  D:\Wpro\backend_cpp\api.cpp ^
  D:\Wpro\backend_cpp\db.cpp ^
  D:\Wpro\backend_cpp\http.cpp ^
  D:\Wpro\backend_cpp\pest.cpp ^
  D:\Wpro\backend_cpp\sensor.cpp ^
  D:\Wpro\backend_cpp\analyze.cpp ^
  D:\Wpro\backend_cpp\alert_queue.cpp ^
  -LD:\mingw64\opt\lib -lsqlite3 -lws2_32 -static -std=c++11

copy /y D:\Wpro\server_new.exe D:\Wpro\backend_cpp\server.exe
`

## Android APK 打包

`cmd
cd /d D:\Wpro\smart_agri_app
npx cap sync
npx cap open android
# 在 Android Studio 中 Build > Build Bundle(s) / APK(s)
`

## 版本

- **v2.0** - 2026-06-18
