#include "MqttExposer.h"

#include "MqttConfig.h"

using namespace coro_http;

MqttExposer::MqttExposer() : is_running_(false) {}

bool MqttExposer::run() {
    if (is_running_) {
        SPDLOG_WARN("MQTT Exposer is already running");
        return false;
    }

    if (!MqttConfig::getInstance()->exposer_enable()) {
        SPDLOG_INFO("MQTT Exposer is disabled in configuration");
        return true;
    }

    init_mqtt_active_connections_metric();

    http_server_ = std::make_unique<coro_http_server>(
        MqttConfig::getInstance()->exposer_thread_count(),
        MqttConfig::getInstance()->exposer_port(),
        MqttConfig::getInstance()->exposer_address());

    http_server_->set_http_handler<GET, POST>(
        "/metrics", [](coro_http_request& req, coro_http_response& resp) {
            std::string mqtt_metrics_data;
            auto mqtt_static_metrics =
                mqtt_static_metric_manager::instance().collect();

            for (auto& m : mqtt_static_metrics) {
                m->serialize(mqtt_metrics_data);
            }

            auto mqtt_dynamic_metrics =
                mqtt_dynamic_metric_manager::instance().collect();

            for (auto& m : mqtt_dynamic_metrics) {
                m->serialize(mqtt_metrics_data);
            }

            resp.set_status_and_content(status_type::ok,
                                        std::move(mqtt_metrics_data));
        });

    future_ = std::make_unique<async_simple::Future<std::error_code>>(
        http_server_->async_start());
    if (future_->hasResult()) {
        SPDLOG_ERROR("Failed to start MQTT Exposer: {}",
                     future_->result().value().message());
        http_server_.reset();
        future_.reset();
        return false;
    }

    is_running_ = true;

    SPDLOG_INFO("MQTT Exposer started successfully");
    return true;
}

void MqttExposer::stop() {
    if (is_running_) {
        http_server_->stop();
        future_->wait();
        is_running_ = false;

        SPDLOG_INFO("MQTT Exposer stopped successfully");
    }
}

void MqttExposer::init_mqtt_active_connections_metric() {
    mqtt_dynamic_metric_manager::instance()
        .create_metric_dynamic<ylt::metric::dynamic_gauge_1t>(
            "mqtt_active_connections", "Number of active MQTT connections",
            std::array<std::string, 1>{"protocol"});

    auto mqtt_active_connections_metric =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_gauge_1t>(
                "mqtt_active_connections");

    mqtt_active_connections_metric->inc({"mqtt"}, 0);
    mqtt_active_connections_metric->inc({"mqtts"}, 0);
    mqtt_active_connections_metric->inc({"ws"}, 0);
    mqtt_active_connections_metric->inc({"wss"}, 0);
}

void MqttExposer::inc_mqtt_active_connections(MQTT_PROTOCOL protocol) {
    static auto mqtt_active_connections_metric =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_gauge_1t>(
                "mqtt_active_connections");

    if (!mqtt_active_connections_metric) {
        return;
    }

    std::string protocol_str = "unknown";

    switch (protocol) {
        case MQTT_PROTOCOL::MQTT:
            protocol_str = "mqtt";
            break;
        case MQTT_PROTOCOL::MQTTS:
            protocol_str = "mqtts";
            break;
        case MQTT_PROTOCOL::WS:
            protocol_str = "ws";
            break;
        case MQTT_PROTOCOL::WSS:
            protocol_str = "wss";
            break;
        default:
            SPDLOG_WARN("Unknown MQTT protocol");
            protocol_str = "unknown";
            break;
    }

    mqtt_active_connections_metric->inc({std::move(protocol_str)});
}

void MqttExposer::dec_mqtt_active_connections(MQTT_PROTOCOL protocol) {
    static auto mqtt_active_connections_metric =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_gauge_1t>(
                "mqtt_active_connections");

    if (!mqtt_active_connections_metric) {
        return;
    }

    std::string protocol_str = "unknown";

    switch (protocol) {
        case MQTT_PROTOCOL::MQTT:
            protocol_str = "mqtt";
            break;
        case MQTT_PROTOCOL::MQTTS:
            protocol_str = "mqtts";
            break;
        case MQTT_PROTOCOL::WS:
            protocol_str = "ws";
            break;
        case MQTT_PROTOCOL::WSS:
            protocol_str = "wss";
            break;
        default:
            SPDLOG_WARN("Unknown MQTT protocol");
            protocol_str = "unknown";
            break;
    }

    mqtt_active_connections_metric->dec({std::move(protocol_str)});
}