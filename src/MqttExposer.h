#pragma once

#include "MqttCommon.h"
#include "MqttSingleton.h"

struct mqtt_metric_tag {};

using mqtt_static_metric_manager = ylt::metric::static_metric_manager<mqtt_metric_tag>;
using mqtt_dynamic_metric_manager = ylt::metric::dynamic_metric_manager<mqtt_metric_tag>;

class MqttExposer : public MqttSingleton<MqttExposer> {
    friend class MqttSingleton<MqttExposer>;

protected:
    MqttExposer();
    ~MqttExposer() = default;

public:
    bool run();

    void stop();

    void inc_mqtt_active_connections(std::string protocol);

    void dec_mqtt_active_connections(std::string protocol);

private:
    void init_mqtt_active_connections_metric();

private:
    bool is_running_;
    std::unique_ptr<coro_http::coro_http_server> http_server_;
    std::unique_ptr<async_simple::Future<std::error_code>> future_;
};