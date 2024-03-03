#include "MqttLogger.h"

#include "MqttConfig.h"

MqttLogger* MqttLogger::getInstance() {
    static MqttLogger MqttLogger;
    return &MqttLogger;
}

bool MqttLogger::init(const std::string& log_file, long unsigned max_rotateSize,
                      long unsigned max_rotateCount) {
    try {
        spdlog::init_thread_pool(MqttConfig::getInstance()->thread_pool_qsize(),
                                 MqttConfig::getInstance()->thread_count());
        auto console_sink =
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, max_rotateSize, max_rotateCount);
#ifndef NDEBUG
        spdlog::set_default_logger(std::make_shared<spdlog::async_logger>(
            "debug_logger", spdlog::sinks_init_list({console_sink, file_sink}),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest));
#else    // Release
        spdlog::set_default_logger(std::make_shared<spdlog::async_logger>(
            "release_logger", file_sink, spdlog::thread_pool(),
            spdlog::async_overflow_policy::block));
#endif

        switch (SPDLOG_ACTIVE_LEVEL) {
            case SPDLOG_LEVEL_TRACE:
                spdlog::set_level(spdlog::level::trace);
                break;
            case SPDLOG_LEVEL_DEBUG:
                spdlog::set_level(spdlog::level::debug);
                break;
            case SPDLOG_LEVEL_INFO:
                spdlog::set_level(spdlog::level::info);
                break;
            case SPDLOG_LEVEL_WARN:
                spdlog::set_level(spdlog::level::warn);
                break;
            case SPDLOG_LEVEL_ERROR:
                spdlog::set_level(spdlog::level::err);
                break;
            case SPDLOG_LEVEL_CRITICAL:
                spdlog::set_level(spdlog::level::critical);
                break;
            case SPDLOG_LEVEL_OFF:
                spdlog::set_level(spdlog::level::off);
                break;
            default:
                break;
        }

        spdlog::set_pattern("[%Y-%m-%d %T.%f] [%^%l%$] %v");
    } catch (const spdlog::spdlog_ex& ex) {
        std::printf("MqttLogger Init Failed");
        return false;
    }
    return true;
}