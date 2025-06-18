#pragma once

#include "MqttAcl.h"
#include "MqttCommon.h"
#include "MqttSingleton.h"

namespace YAML
{
    class Node;
}

class MqttConfig : public MqttSingleton<MqttConfig> {
    friend class MqttSingleton<MqttConfig>;

protected:
    MqttConfig();
    ~MqttConfig() = default;

public:
    bool parse(const std::string& file_name);

    bool auth(const std::string& username, const std::string& password) const noexcept;

    bool acl_check(const mqtt_acl_rule_t& rule) const noexcept;

    inline uint32_t connect_timeout() const noexcept { return connect_timeout_; }

    inline uint32_t check_timeout_duration() const noexcept { return check_timeout_duration_; }

    inline uint32_t check_waiting_map_duration() const noexcept { return check_waiting_map_duration_; }

    inline uint32_t max_resend_count() const noexcept { return max_resend_count_; }

    inline uint32_t resend_duration() const noexcept { return resend_duration_; }

    inline uint32_t max_waiting_time() const noexcept { return max_waiting_time_; }

    inline bool auth() const noexcept { return auth_; }

    inline uint32_t max_packet_size() const noexcept { return max_packet_size_; }

    inline uint32_t max_subscriptions() const noexcept { return max_subscriptions_; }

    inline std::pair<double, double> sub_rate_limit() const noexcept { return sub_rate_limit_; }

    inline std::pair<double, double> pub_rate_limit() const noexcept { return pub_rate_limit_; }

    inline std::string name() const noexcept { return name_; }

    inline uint32_t max_rotate_size() const noexcept { return max_rotate_size_; }

    inline uint32_t max_rotate_count() const noexcept { return max_resend_count_; }

    inline uint32_t thread_pool_qsize() const noexcept { return thread_pool_qsize_; }

    inline uint32_t thread_count() const noexcept { return thread_count_; }

    inline bool acl_enable() const noexcept { return enable_; }

    inline const auto& auto_subscribe_list() const noexcept { return auto_subscribe_list_; }

    inline const auto& listeners() const noexcept { return listeners_; }

    inline bool exposer_enable() const noexcept { return exposer_cfg_.enable; }

    inline std::string exposer_address() const noexcept {
        return exposer_cfg_.address;
    }

    inline uint16_t exposer_port() const noexcept {
        return exposer_cfg_.port;
    }

    inline uint32_t exposer_thread_count() const noexcept {
        return exposer_cfg_.thread_count;
    }

private:
    void parse_listeners(const YAML::Node& node);

    void parse_limits(const YAML::Node& node);

private:
    mqtt_ssl_cfg_t default_ssl_cfg_;
    std::vector<mqtt_listener_cfg_t> listeners_;
    uint32_t connect_timeout_;
    uint32_t check_timeout_duration_;
    uint32_t check_waiting_map_duration_;
    uint32_t max_resend_count_;
    uint32_t resend_duration_;
    uint32_t max_waiting_time_;
    bool auth_;
    uint32_t max_packet_size_;
    uint32_t max_subscriptions_;
    std::pair<double, double> sub_rate_limit_;
    std::pair<double, double> pub_rate_limit_;
    std::string name_;
    uint32_t max_rotate_size_;
    uint32_t max_rotate_count_;
    uint32_t thread_pool_qsize_;
    uint32_t thread_count_;
    std::unordered_map<std::string, std::string> credentials_;
    bool enable_;
    std::string acl_file_;
    bool default_;
    MqttAcl acl_;
    std::list<std::pair<std::string, uint8_t>> auto_subscribe_list_;
    mqtt_exposer_cfg_t exposer_cfg_;
};