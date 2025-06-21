#include "MqttExposer.h"

#include "MqttConfig.h"

using namespace coro_http;

MqttExposer::MqttExposer() : is_running_(false) {}

// clang-format off
const static std::string s_mqtt_active_connections      = "mqtt_active_connections";
const static std::string s_mqtt_sub_topic_count         = "mqtt_sub_topic_count";
const static std::string s_mqtt_unsub_topic_count       = "mqtt_unsub_topic_count";
const static std::string s_mqtt_pub_topic_count         = "mqtt_pub_topic_count";
// clang-format on

// use RVO
static std::string s_get_mqtt_protocol_str(MQTT_PROTOCOL protocol) {
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
            break;
    }

    return protocol_str;
}

static std::string s_get_mqtt_qos_str(MQTT_QUALITY qos) {
    std::string qos_str = "unknown";

    switch (qos) {
        case MQTT_QUALITY::Qos0:
            qos_str = "Qos0";
            break;
        case MQTT_QUALITY::Qos1:
            qos_str = "Qos1";
            break;
        case MQTT_QUALITY::Qos2:
            qos_str = "Qos2";
            break;
        default:
            SPDLOG_WARN("Unknown MQTT sub quality");
            break;
    }

    return qos_str;
}

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
    init_mqtt_pub_topic_count_metric();
    init_mqtt_sub_topic_count_metric();
    init_mqtt_unsub_topic_count_metric();

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
            s_mqtt_active_connections, "Number of active MQTT connections",
            std::array<std::string, 1>{"protocol"});

    mqtt_active_connections_metric_ =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_gauge_1t>(
                s_mqtt_active_connections);

    mqtt_active_connections_metric_->inc({s_get_mqtt_protocol_str(MQTT_PROTOCOL::MQTT)}, 0);
    mqtt_active_connections_metric_->inc({s_get_mqtt_protocol_str(MQTT_PROTOCOL::MQTTS)}, 0);
    mqtt_active_connections_metric_->inc({s_get_mqtt_protocol_str(MQTT_PROTOCOL::WS)}, 0);
    mqtt_active_connections_metric_->inc({s_get_mqtt_protocol_str(MQTT_PROTOCOL::WSS)}, 0);
}

void MqttExposer::init_mqtt_pub_topic_count_metric() {
    mqtt_dynamic_metric_manager::instance()
        .create_metric_dynamic<ylt::metric::dynamic_counter_1t>(
            s_mqtt_pub_topic_count, "Number of publish MQTT topics",
            std::array<std::string, 1>{"quality"});

    mqtt_pub_topic_count_metric_ =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_counter_1t>(
                s_mqtt_pub_topic_count);
    mqtt_dynamic_metric_manager::instance()
        .get_metric_dynamic<ylt::metric::dynamic_counter_1t>(
            s_mqtt_pub_topic_count);

    mqtt_pub_topic_count_metric_->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos0)}, 0);
    mqtt_pub_topic_count_metric_->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos1)}, 0);
    mqtt_pub_topic_count_metric_->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos2)}, 0);
}

void MqttExposer::init_mqtt_sub_topic_count_metric() {
    mqtt_dynamic_metric_manager::instance()
        .create_metric_dynamic<ylt::metric::dynamic_counter_1t>(
            s_mqtt_sub_topic_count, "Number of subscribe MQTT topics",
            std::array<std::string, 1>{"quality"});

    mqtt_sub_topic_count_metric_ =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_counter_1t>(
                s_mqtt_sub_topic_count);

    mqtt_sub_topic_count_metric_->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos0)}, 0);
    mqtt_sub_topic_count_metric_->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos1)}, 0);
    mqtt_sub_topic_count_metric_->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos2)}, 0);
}

void MqttExposer::init_mqtt_unsub_topic_count_metric() {
    mqtt_dynamic_metric_manager::instance()
        .create_metric_dynamic<ylt::metric::dynamic_counter_1t>(
            s_mqtt_unsub_topic_count, "Number of unsubscribe MQTT topics",
            std::array<std::string, 1>{"quality"});

    auto mqtt_unsub_topic_count_metric =
        mqtt_dynamic_metric_manager::instance()
            .get_metric_dynamic<ylt::metric::dynamic_counter_1t>(
                s_mqtt_unsub_topic_count);

    mqtt_unsub_topic_count_metric->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos0)}, 0);
    mqtt_unsub_topic_count_metric->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos1)}, 0);
    mqtt_unsub_topic_count_metric->inc({s_get_mqtt_qos_str(MQTT_QUALITY::Qos2)}, 0);
}

void MqttExposer::inc_mqtt_active_connections(MQTT_PROTOCOL protocol) {
    if (!mqtt_active_connections_metric_) {
        return;
    }

    mqtt_active_connections_metric_->inc({s_get_mqtt_protocol_str(protocol)});
}

void MqttExposer::dec_mqtt_active_connections(MQTT_PROTOCOL protocol) {
    if (!mqtt_active_connections_metric_) {
        return;
    }

    mqtt_active_connections_metric_->dec({s_get_mqtt_protocol_str(protocol)});
}

void MqttExposer::inc_mqtt_pub_topic_count_metric(MQTT_QUALITY qos) {
    if (!mqtt_pub_topic_count_metric_) {
        return;
    }

    mqtt_pub_topic_count_metric_->inc({s_get_mqtt_qos_str(qos)});
}

void MqttExposer::inc_mqtt_sub_topic_count_metric(MQTT_QUALITY qos) {
    if (!mqtt_sub_topic_count_metric_) {
        return;
    }

    mqtt_sub_topic_count_metric_->inc({s_get_mqtt_qos_str(qos)});
}

void MqttExposer::inc_mqtt_unsub_topic_count_metric(MQTT_QUALITY qos) {
    if (!mqtt_unsub_topic_count_metric_) {
        return;
    }

    mqtt_unsub_topic_count_metric_->inc({s_get_mqtt_qos_str(qos)});
}