#pragma once

#include "MqttCommon.h"
#include "MqttAcl.h"

namespace YAML
{
    class Node;
}

class MqttConfig {
public:
    static MqttConfig* getInstance();

    bool parse(const std::string& file_name);

    bool auth(const std::string& username, const std::string& password) {
        if (credentials_.empty()) {
            return true;
        }
        return credentials_.count(username) && credentials_[username] == password;
    };

    inline uint32_t connect_timeout() const { return connect_timeout_; }

    inline uint32_t check_timeout_duration() const { return check_timeout_duration_; }

    inline uint32_t check_waiting_map_duration() const { return check_waiting_map_duration_; }

    inline uint32_t max_resend_count() const { return max_resend_count_; }

    inline uint32_t resend_duration() const { return resend_duration_; }

    inline uint32_t max_waiting_time() const { return max_waiting_time_; }

    inline bool auth() const { return auth_; }

    inline std::string name() const { return name_; }

    inline uint32_t max_rotate_size() const { return max_rotate_size_; }

    inline uint32_t max_rotate_count() const { return max_resend_count_; }

    inline uint32_t thread_pool_qsize() const { return thread_pool_qsize_; }

    inline uint32_t thread_count() const { return thread_count_; }

    inline bool acl_enable() const { return enable_; }

    bool acl_check(const mqtt_acl_rule_t& rule);

    inline const auto& auto_subscribe_list() const { return auto_subscribe_list_; }

    inline const auto& listeners() const { return listeners_; }

private:
    MqttConfig();
    ~MqttConfig() = default;
    MqttConfig(const MqttConfig&) = delete;
    MqttConfig& operator=(const MqttConfig&) = delete;
    MqttConfig(MqttConfig&&) = delete;
    MqttConfig& operator=(MqttConfig&&) = delete;

    void parse_listeners(YAML::Node& node);

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
};