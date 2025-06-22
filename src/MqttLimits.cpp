#include "MqttLimits.h"

#include "yaml-cpp/yaml.h"

MqttLimits::MqttLimits() : enable_(false) {}

bool MqttLimits::load_limits(const std::string& limits_file) {
    try {
        YAML::Node root = YAML::LoadFile(limits_file);

        std::unordered_set<std::string> group_name_set;

        for (const auto& limit_group : root["limit_groups"]) {
            mqtt_limit_group_cfg_t limit_group_cfg;

            limit_group_cfg.name = limit_group["name"].as<std::string>();

            if (limit_group_cfg.name.empty() ||
                group_name_set.contains(limit_group_cfg.name)) {
                throw std::runtime_error("invalid limit group name: " +
                                         limit_group_cfg.name);
            }

            if (limit_group["selector"].IsDefined()) {
                const auto& nodeSelector = limit_group["selector"];

                mqtt_limit_selector_cfg_t limit_selector_cfg;

                if (nodeSelector["client_id"].IsDefined()) {
                    limit_selector_cfg.client_id =
                        nodeSelector["client_id"]
                            .as<std::vector<std::string>>();
                }

                if (nodeSelector["client_id_prefix"].IsDefined()) {
                    limit_selector_cfg.client_id_prefix =
                        nodeSelector["client_id_prefix"]
                            .as<std::vector<std::string>>();
                }

                if (nodeSelector["client_id_regex"].IsDefined()) {
                    limit_selector_cfg.client_id_regex =
                        nodeSelector["client_id_regex"]
                            .as<std::vector<std::string>>();
                }

                if (limit_group_cfg.name != "default") {
                    limit_group_cfg.selector =
                        std::make_unique<mqtt_limit_selector_cfg_t>(
                            limit_selector_cfg);
                }
            }

            if (limit_group["limit"].IsDefined()) {
                const auto& nodeLimit = limit_group["limit"];

                if (nodeLimit["pub_rate"].IsDefined()) {
                    double pub_rate = nodeLimit["pub_rate"].as<double>();
                    pub_rate_limitor_[limit_group_cfg.name] =
                        std::make_unique<MqttTokenBucket>(pub_rate);
                }

                if (nodeLimit["sub_rate"].IsDefined()) {
                    double sub_rate = nodeLimit["sub_rate"].as<double>();
                    sub_rate_limitor_[limit_group_cfg.name] =
                        std::make_unique<MqttTokenBucket>(sub_rate);
                }
            }

            if (limit_group_cfg.name != "default") {
                limit_groups_cfg_.emplace_back(std::move(limit_group_cfg));
            }
        }

    } catch (const std::exception& e) {
        SPDLOG_ERROR("limits file config error: [{}]", e.what());
        return false;
    }
    return true;
}

bool MqttLimits::check_pub_limit(const std::string& client_id) {
    if (!enable_) return false;

    std::string group_name = get_group_name(client_id);

    auto it = pub_rate_limitor_.find(group_name);
    if (it == pub_rate_limitor_.end() || it->second == nullptr) {
        return false;
    }

    return it->second->tryConsume();
}

bool MqttLimits::check_sub_limit(const std::string& client_id) {
    if (!enable_) return false;

    std::string group_name = get_group_name(client_id);

    auto it = sub_rate_limitor_.find(group_name);
    if (it == sub_rate_limitor_.end() || it->second == nullptr) {
        return false;
    }

    return it->second->tryConsume();
}

std::string MqttLimits::get_group_name(const std::string& client_id) {
    std::string name = "default";

    for (const auto& limit_group_cfg : limit_groups_cfg_) {
        if (limit_group_cfg.selector) {
            for (const auto& cid : limit_group_cfg.selector->client_id) {
                if (client_id == cid) {
                    name = limit_group_cfg.name;
                    return name;
                }
            }

            for (const auto& cid_prefix :
                 limit_group_cfg.selector->client_id_prefix) {
                if (client_id.starts_with(cid_prefix)) {
                    name = limit_group_cfg.name;
                    return name;
                }
            }

            for (const auto& cid_regex :
                 limit_group_cfg.selector->client_id_regex) {
                try {
                    if (std::regex_match(client_id, std::regex(cid_regex))) {
                        name = limit_group_cfg.name;
                        return name;
                    }
                } catch (std::exception& e) {
                    SPDLOG_ERROR("client_id_regex match error: [{}]", e.what());
                }
            }

        } else {
            name = limit_group_cfg.name;
            return name;
        }
    }

    return name;
}