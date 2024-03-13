#include "MqttBroker.h"

MqttBroker::MqttBroker() : gen_sid_counter(0) {}

bool MqttBroker::join_or_update(std::shared_ptr<MqttSession> session) {
    bool session_present = false;
    std::string sid = session->get_session_id();

    auto iter = session_map.find(sid);
    if (iter != session_map.end()) {
        // 会话状态恢复
        auto old_session = iter->second;
        old_session->move_session_state(session);
        session_present = true;
    }

    session_map[sid] = session;
    return session_present;
}

void MqttBroker::leave(const std::string& sid) { session_map.erase(sid); }

void MqttBroker::dispatch(const mqtt_packet_t& packet) {
    SPDLOG_DEBUG("dispatch packet, session_map size = [{}]",
                 session_map.size());

    // 将消息分发给拥有订阅项的会话
    for (auto& sid : MqttSession::active_sub_set) {
        session_map[sid]->push_packet(packet);
    }
}

void MqttBroker::dispatch_will(const mqtt_packet_t& packet,
                               const std::string& sid) {
    // 遗嘱消息不发送给已经死去的会话, 虽然死了但还是可能保留了会话状态
    // 会继续接收主题消息
    for (auto& session_id : MqttSession::active_sub_set) {
        if (session_id != sid) {
            session_map[sid]->push_packet(packet);
        }
    }
}

void MqttBroker::add_retain(const mqtt_packet_t& packet) {
    retain_map[*packet.topic_name] = packet;
}

void MqttBroker::get_retain(std::shared_ptr<MqttSession> session,
                            const std::string& sub_topic) {
    for (auto& [pub_topic, packet] : retain_map) {
        // 指定保留消息要发送给哪个主题
        packet.specified_topic_name = sub_topic;
        session->push_packet(packet);
    }
}

void MqttBroker::remove_retain(const std::string& topic_name) {
    retain_map.erase(topic_name);
}

std::string MqttBroker::gen_session_id() {
    std::string sid;
    do {
        sid = "MS_" + std::to_string(gen_sid_counter);
        gen_sid_counter++;
    } while (session_map.contains(sid));

    return sid;
}