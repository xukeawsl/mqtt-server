#pragma once

#include "MqttSessionState.h"

class MqttBroker;

class MqttSession: public std::enable_shared_from_this<MqttSession> {
public:

#ifdef MQ_WITH_TLS
    MqttSession(asio::ssl::stream<asio::ip::tcp::socket> ssl_socket, MqttBroker& broker);
#else
    MqttSession(asio::ip::tcp::socket client_socket, MqttBroker& mqtt_broker);
#endif

    ~MqttSession();

    void start();

    std::string get_session_id();

    void move_session_state(std::shared_ptr<MqttSession> old_session);

    void push_packet(const mqtt_packet_t& packet);

private:
#ifdef MQ_WITH_TLS
    asio::awaitable<void> handle_handshake();
#endif

    void handle_session();

    void disconnect();

    void init_buffer();

    void flush_deadline();

    void handle_error_code();

    uint16_t gen_packet_id();

    asio::awaitable<void> handle_keep_alive();

    asio::awaitable<void> handle_packet();

    asio::awaitable<MQTT_RC_CODE> handle_connect();

    asio::awaitable<MQTT_RC_CODE> handle_publish();

    asio::awaitable<MQTT_RC_CODE> handle_puback();

    asio::awaitable<MQTT_RC_CODE> handle_pubrec();

    asio::awaitable<MQTT_RC_CODE> handle_pubrel();

    asio::awaitable<MQTT_RC_CODE> handle_pubcomp();

    asio::awaitable<MQTT_RC_CODE> handle_subscribe();

    asio::awaitable<MQTT_RC_CODE> handle_unsubscribe();

    asio::awaitable<MQTT_RC_CODE> handle_pingreq();

    asio::awaitable<MQTT_RC_CODE> handle_disconnect();

    asio::awaitable<MQTT_RC_CODE> read_byte(uint8_t* addr, bool record_pos);

    asio::awaitable<MQTT_RC_CODE> read_uint16(uint16_t* addr, bool record_pos);

    asio::awaitable<MQTT_RC_CODE> read_bytes_to_buf(std::string& bytes, uint32_t n, bool record_pos);

    asio::awaitable<MQTT_RC_CODE> read_uint16_header_length_bytes(std::string& bytes, bool record_pos);

    asio::awaitable<MQTT_RC_CODE> read_utf8_string(std::string& str, bool record_pos);

    asio::awaitable<MQTT_RC_CODE> read_will_packet(mqtt_packet_t& packet, bool record_pos);

    MQTT_RC_CODE add_mqtt_fixed_header(std::string& packet, uint8_t cmd, uint32_t remaining_length);

    MQTT_RC_CODE check_command();

    MQTT_RC_CODE check_remaining_length();

    MQTT_RC_CODE check_sub_topic(const std::string& topic_name);

    MQTT_RC_CODE check_pub_topic(const std::string& topic_name);

    MQTT_RC_CODE check_validate_utf8(const std::string& ustr);

    asio::awaitable<MQTT_RC_CODE> read_remaining_length();

    asio::awaitable<MQTT_RC_CODE> read_fixed_header();

    asio::awaitable<MQTT_RC_CODE> send_connack(uint8_t ack, uint8_t reason_code);

    asio::awaitable<MQTT_RC_CODE> send_suback(uint16_t packet_id, const std::string& payload);

    asio::awaitable<MQTT_RC_CODE> send_unsuback(uint16_t packet_id);

    asio::awaitable<MQTT_RC_CODE> send_puback(uint16_t packet_id);

    asio::awaitable<MQTT_RC_CODE> send_pubrec(uint16_t packet_id);

    asio::awaitable<MQTT_RC_CODE> send_pubrel(uint16_t packet_id);

    asio::awaitable<MQTT_RC_CODE> send_pubcomp(uint16_t packet_id);

    asio::awaitable<MQTT_RC_CODE> send_pingresp();

    asio::awaitable<MQTT_RC_CODE> handle_inflighting_packets();

    asio::awaitable<MQTT_RC_CODE> handle_waiting_map_packets();

    asio::awaitable<MQTT_RC_CODE> send_mqtt_packets(const std::list<mqtt_packet_t>& packet_list);

    asio::awaitable<void> send_publish_qos0(mqtt_packet_t packet);

    asio::awaitable<MQTT_RC_CODE> send_publish_qos1(mqtt_packet_t packet, bool is_new);

    asio::awaitable<MQTT_RC_CODE> send_publish_qos2(mqtt_packet_t packet, bool is_new);

    void add_subscribe(const std::list<std::pair<std::string, uint8_t>>& sub_topic_list);

public:
    // 统计存在订阅项的会话, 优化消息分发的性能
    static std::unordered_set<std::string> active_sub_set;

private:
#ifdef MQ_WITH_TLS
    asio::ssl::stream<asio::ip::tcp::socket> socket;
#else
    asio::ip::tcp::socket socket;
#endif
    std::string username;
    std::string client_id;
    MqttBroker& broker;
    asio::steady_timer cond_timer;
    asio::steady_timer keep_alive_timer;
    asio::steady_timer check_timer;
    asio::experimental::channel<void()> write_lock;
    MqttSessionState session_state;
    bool complete_connect;
    MQTT_RC_CODE rc;
    uint8_t command;
    uint32_t pos;
    uint32_t remaining_length;
    std::chrono::steady_clock::time_point deadline;
};