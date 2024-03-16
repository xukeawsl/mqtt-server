# mqtt-server 简介
[![License](https://img.shields.io/npm/l/mithril.svg)](https://github.com/xukeawsl/mqtt-server/blob/master/LICENSE)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/af9c4bb5e8ec479d9c4e2c76ba2ad6e2)](https://app.codacy.com/gh/xukeawsl/mqtt-server/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Build status](https://ci.appveyor.com/api/projects/status/kw4kntnok7iab55b?svg=true)](https://ci.appveyor.com/project/xukeawsl/mqtt-server)

mqtt-server 是一个使用 C++20 协程开发的支持 MQTT `v3.1.1` 协议的高性能 MQTT Broker,
支持单机上万的并发连接, CPU 及内存占用小, 可以很好的运行在资源有限的机器上,
支持 Linux 和 Windows 平台

## 前提

* 需要支持 C++20 的编译器版本(Linux GCC 11.2+, Windows MinGW 11.2+)

## 特性

* 支持完整的 MQTT `3.1.1` 协议

* 支持 ACL 认证 (详见 [How to use ACL](https://github.com/xukeawsl/mqtt-server/wiki/4.How-to-use-ACL) )

* 支持自动订阅 (为新连接的客户端自动订阅指定主题, 不受 ACL 控制)

* 支持 SSL/TLS 安全通信 (`TLS1.2` 和 `TLS1.3`) (详见 [How to use TLS](https://github.com/xukeawsl/mqtt-server/wiki/2.How-to-use-TLS) )

* 比 `EMQX` 更优秀的性能和更低的资源消耗 (详见 [压测报告](https://github.com/xukeawsl/mqtt-server/tree/master/bench) )

* 支持 Docker 部署 (详见 [Use Docker Image](https://github.com/xukeawsl/mqtt-server/wiki/3.Use-Docker-Image) )

## 使用

### 1. 克隆源代码
```bash
git clone https://github.com/xukeawsl/mqtt-server.git
cd mqtt-server
mkdir build && cd build
```

### 2. Cmake 构建

* Linux 平台

```bash
# 默认 Release 级别, 也可以选择 Debug 级别构建
# cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake ..
cmake --build .

# 构建时可以选择日志级别, Release 构建默认日志级别是 Info
# Debug 构建默认日志级别是 Debug, 支持以下的日志级别
# Trace, Debug, Info, Warn, Error, Critical, Off
cmake -DLOG_LEVEL=Error ..
```

* Windows 平台

```bash
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### 3. 运行 mqtt-server
```bash
# Linux
./mqtt-server

# Windows
.\mqtt-server.exe
```

## 文档

关于如何使用 `SSL/TLS` 或其他详细内容见 [wiki](https://github.com/xukeawsl/mqtt-server/wiki) 页面

## 参考

* https://github.com/mcxiaoke/mqtt

* https://github.com/eclipse/mosquitto