#pragma once

#include "MqttCommon.h"

#include "metrics/IMetrics.h"
#include "metrics/MqttConnMetrics.h"

class MqttMetrics {
public:
    static MqttMetrics* getInstance() {
        static MqttMetrics instance;
        return &instance;
    }

    void init(const std::string& address, uint16_t port);

    MqttConnMetrics& get_mqtt_conn_metrics() {
        return mqtt_conn_metrics_;
    }

private:
    MqttMetrics() = default;
    ~MqttMetrics() = default;
    MqttMetrics(const MqttMetrics&) = delete;
    MqttMetrics& operator=(const MqttMetrics&) = delete;
    MqttMetrics(MqttMetrics&&) = delete;
    MqttMetrics& operator=(MqttMetrics&&) = delete;

    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;

    MqttConnMetrics mqtt_conn_metrics_;
};