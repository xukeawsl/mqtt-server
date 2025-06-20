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

    void inc_mqtt_active_connections(MQTT_PROTOCOL protocol);

    void dec_mqtt_active_connections(MQTT_PROTOCOL protocol);

    void inc_mqtt_pub_topic_count_metric(MQTT_QUALITY qos);

    void inc_mqtt_sub_topic_count_metric(MQTT_QUALITY qos);

    void inc_mqtt_unsub_topic_count_metric(MQTT_QUALITY qos);

private:
    void init_mqtt_active_connections_metric();

    void init_mqtt_pub_topic_count_metric();

    void init_mqtt_sub_topic_count_metric();

    void init_mqtt_unsub_topic_count_metric();

private:
    bool is_running_;
    std::unique_ptr<coro_http::coro_http_server> http_server_;
    std::unique_ptr<async_simple::Future<std::error_code>> future_;

    // dynamic metrics
    std::shared_ptr<ylt::metric::dynamic_gauge_1t> mqtt_active_connections_metric_;
    std::shared_ptr<ylt::metric::dynamic_counter_1t> mqtt_pub_topic_count_metric_;
    std::shared_ptr<ylt::metric::dynamic_counter_1t> mqtt_sub_topic_count_metric_;
    std::shared_ptr<ylt::metric::dynamic_counter_1t> mqtt_unsub_topic_count_metric_;

    // static metrics
    std::shared_ptr<ylt::metric::counter_t> mqtt_pub_topic_total_count_metric_;
    std::shared_ptr<ylt::metric::counter_t> mqtt_sub_topic_total_count_metric_;
    std::shared_ptr<ylt::metric::counter_t> mqtt_unsub_topic_total_count_metric_;
};