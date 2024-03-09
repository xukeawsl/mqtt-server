#pragma once

#include "MqttCommon.h"
#include "MqttSession.h"

class MqttBroker {
public:
    MqttBroker();

    ~MqttBroker() = default;

    bool join_or_update(std::shared_ptr<MqttSession> session);

    void leave(const std::string& sid);

    void dispatch(const mqtt_packet_t& packet);

    void dispatch_will(const mqtt_packet_t& packet, const std::string& sid);

    void add_retain(const mqtt_packet_t& packet);

    void get_retain(std::shared_ptr<MqttSession> session, const std::string& topic_name);

    void remove_retain(const std::string& topic_name);

    std::string gen_session_id();

private:
    uint32_t gen_sid_counter;
    std::unordered_map<std::string, mqtt_packet_t> retain_map;
    std::unordered_map<std::string, std::shared_ptr<MqttSession>> session_map;
};