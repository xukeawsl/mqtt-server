#pragma once

#include <unordered_map>

#include "metrics/IMetrics.h"

class MqttConnMetrics : public IMetrics {
public:
    void register_metrics(prometheus::Registry& registry) override {
        auto& family = prometheus::BuildGauge()
            .Name("mqtt_active_connections")
            .Help("Number of active MQTT connections")
            .Register(registry);

        protocol_gauges_ = {
            {MQTT_PROTOCOL::MQTT, &family.Add({{"protocol", "mqtt"}})},
            {MQTT_PROTOCOL::MQTTS, &family.Add({{"protocol", "mqtts"}})},
            {MQTT_PROTOCOL::WS, &family.Add({{"protocol", "ws"}})},
            {MQTT_PROTOCOL::WSS, &family.Add({{"protocol", "wss"}})}
        };

        is_registered_ = true;
    }

    void increment(MQTT_PROTOCOL protocol) {
        if (!is_registered_) return;
        protocol_gauges_[protocol]->Increment();
    }

    void decrement(MQTT_PROTOCOL protocol) {
        if (!is_registered_) return;
        protocol_gauges_[protocol]->Decrement();
    }

private:
    bool is_registered_ = false;
    std::unordered_map<MQTT_PROTOCOL, prometheus::Gauge*> protocol_gauges_;
};