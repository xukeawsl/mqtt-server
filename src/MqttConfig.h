#pragma once

#include <string>

class MqttConfig {
public:
    static MqttConfig* getInstance();

    bool parse(const std::string& file_name);

    inline std::string address() const { return address_; }

    inline uint16_t port() const { return port_; }

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

private:
    MqttConfig();
    ~MqttConfig() = default;
    MqttConfig(const MqttConfig&) = delete;
    MqttConfig& operator=(const MqttConfig&) = delete;
    MqttConfig(MqttConfig&&) = delete;
    MqttConfig& operator=(MqttConfig&&) = delete;

private:
    std::string address_;
    uint16_t port_;
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
};