#pragma once

#include "MqttCommon.h"

struct MqttSessionState {
    bool clean_session;
    uint16_t keep_alive;
    uint16_t packet_id_gen;
    mqtt_packet_t will_topic;
    std::unordered_map<std::string, uint8_t> sub_topic_map;
    std::unordered_map<uint16_t, mqtt_packet_t> waiting_map;
    std::queue<mqtt_packet_t> inflight_queue;

    MqttSessionState() :
        clean_session(true),
        keep_alive(0U),
        packet_id_gen(0U) {}
};