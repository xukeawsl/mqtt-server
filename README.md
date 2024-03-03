# mqtt-server 简介
[![License](https://img.shields.io/npm/l/mithril.svg)](https://github.com/xukeawsl/mqtt-server/blob/master/LICENSE)

mqtt-server 是一个使用 C++20 协程开发的支持 MQTT `v3.1.1` 协议的高性能 MQTT Broker,
支持单机上万的并发连接, CPU 及内存占用小, 可以很好的运行在资源有限的机器上

## 前提

* 需要支持 C++20 的编译器版本(g++ 11.2)

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
```