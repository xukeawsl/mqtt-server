#pragma once

#include "MqttCommon.h"

template <typename SocketType>
class MqttSession;

template <typename SocketType, typename SslSocketType = void>
class MqttBroker {
public:
    MqttBroker();

    ~MqttBroker() = default;

    bool join_or_update(std::shared_ptr<MqttSession<SocketType>> session);

    void get_retain(std::shared_ptr<MqttSession<SocketType>> session, const std::string& topic_name);

#ifdef MQ_WITH_TLS
    bool join_or_update(std::shared_ptr<MqttSession<SslSocketType>> session);

    void get_retain(std::shared_ptr<MqttSession<SslSocketType>> session, const std::string& topic_name);
#endif

    void leave(const std::string& sid);

    void dispatch(const mqtt_packet_t& packet);

    void dispatch_will(const mqtt_packet_t& packet, const std::string& sid);

    void add_retain(const mqtt_packet_t& packet);

    void remove_retain(const std::string& topic_name);

    std::string gen_session_id();

private:
    uint32_t gen_sid_counter;
    std::unordered_map<std::string, mqtt_packet_t> retain_map;
    std::unordered_map<std::string, std::shared_ptr<MqttSession<SocketType>>> session_map;
#ifdef MQ_WITH_TLS
    std::unordered_map<std::string, std::shared_ptr<MqttSession<SslSocketType>>> ssl_session_map;
#endif
};

#include "MqttBroker.ipp"