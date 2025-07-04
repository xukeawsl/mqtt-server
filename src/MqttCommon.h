#pragma once

#include <list>
#include <regex>
#include <queue>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>

#include "sha1.hpp"
#include "asio.hpp"
#include "asio/experimental/channel.hpp"
#include "ylt/metric/counter.hpp"
#include "ylt/metric/gauge.hpp"
#include "ylt/metric/metric_manager.hpp"
#include "ylt/coro_http/coro_http_server.hpp"

#ifdef MQ_WITH_TLS

#include "asio/ssl.hpp"

#endif

#include "MqttLogger.h"

enum class MQTT_PROTOCOL: uint8_t {
    MQTT,
    MQTTS,
    WS,
    WSS,
};

enum class MQTT_QUALITY : uint8_t {
    Qos0,
    Qos1,
    Qos2,
    Unknown
};

enum class MQTT_MSG_STATE: uint8_t {
    INVALID = 0,
    WAIT_RESEND_PUBLISH_QOS1,
    WAIT_RESEND_PUBLISH_QOS2,
    WAIT_RESEND_PUBREL,
    WAIT_RESEND_PUBCOMP,
    WAIT_RECEIVE_PUBACK,
    WAIT_RECEIVE_PUBREC,
    WAIT_RECEIVE_PUBREL,
    WAIT_RECEIVE_PUBCOMP,
};

enum class MQTT_RC_CODE: uint8_t {
    ERR_SUCCESS = 0,
    ERR_SUCCESS_DISCONNECT,
    ERR_NO_CONN,
    ERR_DUP_CONNECT,
    ERR_NOT_CONNECT,
    ERR_COMMAND,
    ERR_COMMAND_RESERVED,
    ERR_REMAINING_LENGTH,
    ERR_PROTOCOL,
    ERR_MALFORMED_UTF8,
    ERR_MALFORMED_PACKET,
    ERR_NOT_SUPPORTED,
    ERR_PAYLOAD_SIZE,
    ERR_STR_LENGTH_UTF8,
    ERR_SUB_TOPIC_NAME,
    ERR_PUB_TOPIC_NAME,
    ERR_BAD_USERNAME_PASSWORD,
    ERR_BAD_CLIENT_ID,
    ERR_REFUSED_NOT_AUTHORIZED,
};

struct MQTT_CMD {
    static constexpr uint8_t CONNECT = 0x10U;
    static constexpr uint8_t CONNACK = 0x20U;
    static constexpr uint8_t PUBLISH = 0x30U;
    static constexpr uint8_t PUBACK = 0x40U;
    static constexpr uint8_t PUBREC = 0x50U;
    static constexpr uint8_t PUBREL = 0x60U;
    static constexpr uint8_t PUBCOMP = 0x70U;
    static constexpr uint8_t SUBSCRIBE = 0x80U;
    static constexpr uint8_t SUBACK = 0x90U;
    static constexpr uint8_t PUBLIMIT = 0x93U;
    static constexpr uint8_t UNSUBSCRIBE = 0xA0U;
    static constexpr uint8_t UNSUBACK = 0xB0U;
    static constexpr uint8_t PINGREQ = 0xC0U;
    static constexpr uint8_t PINGRESP = 0xD0U;
    static constexpr uint8_t DISCONNECT = 0xE0U;
};

struct MQTT_CONNACK {
    static constexpr uint8_t ACCEPTED = 0x00U;
    static constexpr uint8_t REFUSED_PROTOCOL_VERSION = 0x01U;
    static constexpr uint8_t REFUSED_IDENTIFIER_REJECTED = 0x02U;
    static constexpr uint8_t REFUSED_SERVER_UNAVAILABLE = 0x03U;
    static constexpr uint8_t REFUSED_BAD_USERNAME_PASSWORD = 0x04U;
    static constexpr uint8_t REFUSED_NOT_AUTHORIZED = 0x05U;
};

struct mqtt_packet_t {
    struct {
        uint8_t qos: 2;
        uint8_t dup: 1;
        uint8_t retain: 1;
    };
    
    MQTT_MSG_STATE state;
    uint16_t packet_id;
    uint32_t max_resend_count;
    std::string specified_topic_name;
    std::shared_ptr<const std::string> topic_name;
    std::shared_ptr<const std::string> payload;
    std::chrono::time_point<std::chrono::steady_clock> expiry_time;

    mqtt_packet_t() :
        qos(0b11U),
        dup(0b00U),
        retain(0b00U),
        state(MQTT_MSG_STATE::INVALID),
        max_resend_count(0U),
        topic_name(std::make_shared<const std::string>("")),
        payload(std::make_shared<const std::string>(""))
        {}
};

enum class MQTT_ACL_STATE: uint8_t {
    NONE,
    ALLOW,
    DENY,
};

enum class MQTT_ACL_TYPE: uint8_t {
    USERNAME,
    IPADDR,
    CLIENTID,
};

enum class MQTT_ACL_MODE: uint8_t {
    EQ,
    RE,
};

enum class MQTT_ACL_ACTION: uint8_t {
    SUB,
    PUB,
    ALL,
};

struct mqtt_acl_rule_t {
    MQTT_ACL_STATE permission;
    MQTT_ACL_TYPE type;
    std::string object;
    MQTT_ACL_MODE mode;
    MQTT_ACL_ACTION action;
    std::unique_ptr<std::unordered_set<std::string>> topics;

    mqtt_acl_rule_t():
        permission(MQTT_ACL_STATE::NONE),
        type(MQTT_ACL_TYPE::USERNAME),
        mode(MQTT_ACL_MODE::EQ),
        action(MQTT_ACL_ACTION::ALL)
        {}
};

enum class MQTT_SSL_VERSION: uint8_t {
    TLSv12,
    TLSv13,
};

enum class MQTT_SSL_VERIFY: uint8_t {
    NONE,
    PEER,
};

struct mqtt_ssl_cfg_t {
    MQTT_SSL_VERSION version;
    std::string cacertfile;
    std::string certfile;
    std::string keyfile;
    std::string password;
    MQTT_SSL_VERIFY verify_mode;
    bool fail_if_no_peer_cert;
    std::string dhparam;
};

struct mqtt_listener_cfg_t {
    MQTT_PROTOCOL proto;
    std::string address;
    uint16_t port;
    mqtt_ssl_cfg_t ssl_cfg;
};

struct mqtt_exposer_cfg_t {
    bool enable;
    std::string address;
    uint16_t port;
    uint32_t thread_count;
};

struct mqtt_limit_selector_cfg_t {
    std::vector<std::string> client_id;
    std::vector<std::string> client_id_prefix;
    std::vector<std::string> client_id_regex;
};

struct mqtt_limit_group_cfg_t {
    std::string name;
    std::unique_ptr<mqtt_limit_selector_cfg_t> selector;
};