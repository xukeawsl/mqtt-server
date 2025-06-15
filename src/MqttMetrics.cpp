#include "MqttMetrics.h"

void MqttMetrics::init(const std::string& address, uint16_t port) {
    registry_ = std::make_shared<prometheus::Registry>();
    exposer_ = std::make_unique<prometheus::Exposer>(address + ":" +
                                                     std::to_string(port));
    exposer_->RegisterCollectable(registry_);

    mqtt_conn_metrics_.register_metrics(*registry_);

    return;
}