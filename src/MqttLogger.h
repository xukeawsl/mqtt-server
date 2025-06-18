#pragma once

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "MqttSingleton.h"

class MqttLogger: public MqttSingleton<MqttLogger> {
    friend class MqttSingleton<MqttLogger>;

protected:
    MqttLogger() = default;
    ~MqttLogger() = default;

public:
    bool init();

    void shutdown();
};