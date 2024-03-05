# mqtt-server 简介
[![License](https://img.shields.io/npm/l/mithril.svg)](https://github.com/xukeawsl/mqtt-server/blob/master/LICENSE)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/af9c4bb5e8ec479d9c4e2c76ba2ad6e2)](https://app.codacy.com/gh/xukeawsl/mqtt-server/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)

mqtt-server 是一个使用 C++20 协程开发的支持 MQTT `v3.1.1` 协议的高性能 MQTT Broker,
支持单机上万的并发连接, CPU 及内存占用小, 可以很好的运行在资源有限的机器上,
支持 Linux 和 Windows 平台

## 前提

* 需要支持 C++20 的编译器版本(Linux GCC 11.2+, Windows MinGW 11.2+)

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

### 4. 使用 SSL/TLS

* 确保环境上已经安装了 `openssl` , 可以通过 `MQ_WITH_TLS` 选项来开启 `SSL/TLS` 功能

```bash
cmake -DMQ_WITH_TLS=On ..
```