#pragma once

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

class MqttLogger {
public:
    static MqttLogger* getInstance();

    bool init(const std::string& log_file, long unsigned max_rotateSize,
              long unsigned max_rotateCount);

private:
    MqttLogger() = default;
    ~MqttLogger() = default;
    MqttLogger(const MqttLogger&) = delete;
    MqttLogger& operator=(const MqttLogger&) = delete;
    MqttLogger(MqttLogger&&) = delete;
    MqttLogger& operator=(MqttLogger&&) = delete;
};