#pragma once

#include "MqttCommon.h"
#include "MqttSingleton.h"
#include "MqttTokenBucket.h"

class MqttLimits : public MqttSingleton<MqttLimits> {
    friend class MqttSingleton<MqttLimits>;
protected:
    MqttLimits();
    ~MqttLimits() = default;

public:
    bool load_limits(const std::string& limits_file);

    bool check_pub_limit(const std::string& client_id);

    bool check_sub_limit(const std::string& client_id);

private:
    std::string get_group_name(const std::string& client_id);

private:
    bool enable_;
    std::vector<mqtt_limit_group_cfg_t> limit_groups_cfg_;
    std::unordered_map<std::string, std::unique_ptr<MqttTokenBucket>> pub_rate_limitor_;
    std::unordered_map<std::string, std::unique_ptr<MqttTokenBucket>> sub_rate_limitor_;
};