#include "MqttAcl.h"

#include "yaml-cpp/yaml.h"

bool MqttAcl::load_acl(const std::string& acl_file) {
    try {
        YAML::Node root = YAML::LoadFile(acl_file);

        for (const auto& config : root) {
            mqtt_acl_rule_t rule;

            if (!config["permission"].IsDefined()) {
                throw "acl file no permission";
            }

            std::string tmp = config["permission"].as<std::string>();

            if (tmp == "allow")
                rule.permission = MQTT_ACL_STATE::ALLOW;
            else if (tmp == "deny")
                rule.permission = MQTT_ACL_STATE::DENY;
            else {
                throw "acl file permission config error";
            }

            if (!config["type"].IsDefined()) {
                throw "acl file no type";
            }

            tmp = config["type"].as<std::string>();

            if (tmp == "username")
                rule.type = MQTT_ACL_TYPE::USERNAME;
            else if (tmp == "ipaddr")
                rule.type = MQTT_ACL_TYPE::IPADDR;
            else if (tmp == "clientid")
                rule.type = MQTT_ACL_TYPE::CLIENTID;
            else {
                throw "acl file type config error";
            }

            if (!config["object"].IsDefined()) {
                throw "acl file no object";
            }

            rule.object = config["object"].as<std::string>();

            if (!config["mode"].IsDefined()) {
                throw "acl file no mode";
            }

            tmp = config["mode"].as<std::string>();

            if (tmp == "eq")
                rule.mode = MQTT_ACL_MODE::EQ;
            else if (tmp == "re")
                rule.mode = MQTT_ACL_MODE::RE;
            else {
                throw "acl file mode config error";
            }

            if (rule.type == MQTT_ACL_TYPE::USERNAME) {
                if (!config["action"].IsDefined()) {
                    throw "acl file no action";
                }

                tmp = config["action"].as<std::string>();

                if (tmp == "sub")
                    rule.action = MQTT_ACL_ACTION::SUB;
                else if (tmp == "pub")
                    rule.action = MQTT_ACL_ACTION::PUB;
                else if (tmp == "all")
                    rule.action = MQTT_ACL_ACTION::ALL;
                else {
                    throw "acl file action config error";
                }

                if (!config["topics"].IsDefined()) {
                    throw "acl file no topics";
                }

                std::unordered_set<std::string> st;

                for (const auto& topic : config["topics"]) {
                    st.emplace(topic.as<std::string>());
                }

                rule.topics = std::make_unique<std::unordered_set<std::string>>(
                    std::move(st));
            }

            acl_.emplace_back(std::move(rule));
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("acl file config error: [{}]", e.what());
        return false;
    }
    return true;
}

MQTT_ACL_STATE MqttAcl::check_acl(const mqtt_acl_rule_t& rule) {
    for (auto& acl_rule : acl_) {
        auto state = check_acl_detail(acl_rule, rule);
        if (state != MQTT_ACL_STATE::NONE) {
            return state;
        }
    }
    return MQTT_ACL_STATE::NONE;
}

MQTT_ACL_STATE MqttAcl::check_acl_detail(const mqtt_acl_rule_t& acl_rule,
                                         const mqtt_acl_rule_t& rule) {
    if (rule.type != acl_rule.type) {
        return MQTT_ACL_STATE::NONE;
    }

    if (acl_rule.mode == MQTT_ACL_MODE::EQ) {
        if (acl_rule.object != rule.object) {
            return MQTT_ACL_STATE::NONE;
        }
    } else {
        try {
            if (!std::regex_match(rule.object, std::regex(acl_rule.object))) {
                return MQTT_ACL_STATE::NONE;
            }
        } catch (std::exception& e) {
            SPDLOG_ERROR("regex_match error: [{}]", e.what());
            return MQTT_ACL_STATE::NONE;
        }
    }

    if (acl_rule.type == MQTT_ACL_TYPE::USERNAME) {
        if (acl_rule.action != MQTT_ACL_ACTION::ALL &&
            acl_rule.action != rule.action) {
            return MQTT_ACL_STATE::NONE;
        }

        auto topic = *(rule.topics->begin());

        // 对于订阅控制来说只需要进行精确匹配
        if (rule.action == MQTT_ACL_ACTION::SUB) {
            if (acl_rule.topics->contains(topic)) {
                return acl_rule.permission;
            }

            return MQTT_ACL_STATE::NONE;
        }

        // 对于发布控制来说需要进行通配符匹配
        for (const auto& t : *(acl_rule.topics)) {
            if (util::check_topic_match(topic, t)) {
                return acl_rule.permission;
            }
        }

        return MQTT_ACL_STATE::NONE;

    } else if (acl_rule.type == MQTT_ACL_TYPE::IPADDR) {
        return acl_rule.permission;
    } else if (acl_rule.type == MQTT_ACL_TYPE::CLIENTID) {
        return acl_rule.permission;
    }

    return MQTT_ACL_STATE::NONE;
}