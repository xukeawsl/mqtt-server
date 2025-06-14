#pragma once

#include "MqttCommon.h"
#include "MqttUtils.h"

class MqttAcl {
public:
    bool load_acl(const std::string& acl_file);

    MQTT_ACL_STATE check_acl(const mqtt_acl_rule_t& rule) const noexcept;

private:
    MQTT_ACL_STATE check_acl_detail(const mqtt_acl_rule_t& acl_rule, const mqtt_acl_rule_t& rule) const noexcept;

private:
    std::list<mqtt_acl_rule_t> acl_;
};