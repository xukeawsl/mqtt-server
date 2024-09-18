#include "MqttConfig.h"

#include "yaml-cpp/yaml.h"

MqttConfig* MqttConfig::getInstance() {
    static MqttConfig config;
    return &config;
}

MqttConfig::MqttConfig()
    : connect_timeout_(10),
      check_timeout_duration_(1),
      check_waiting_map_duration_(1),
      max_resend_count_(3),
      resend_duration_(60),
      max_waiting_time_(60),
      auth_(false),
      name_("logs/mqtt-server.log"),
      max_rotate_size_(1024 * 1024),
      thread_pool_qsize_(8192),
      thread_count_(1),
      enable_(false),
      default_(false) {
    default_ssl_cfg_.version = MQTT_SSL_VERSION::TLSv12;
    default_ssl_cfg_.verify_mode = MQTT_SSL_VERIFY::NONE;
    default_ssl_cfg_.fail_if_no_peer_cert = false;
}

bool MqttConfig::parse(const std::string& file_name) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(file_name);

        if (!root["ssl"].IsDefined()) {
            throw std::runtime_error("No SSL configuration");
        }

        auto nodeSSL = root["ssl"];

        if (nodeSSL["version"].IsDefined() &&
            nodeSSL["version"].as<std::string>() == "tls1.3") {
            default_ssl_cfg_.version = MQTT_SSL_VERSION::TLSv13;
        }

        if (!nodeSSL["certfile"].IsDefined()) {
            throw std::runtime_error("No SSL certfile");
        }

        default_ssl_cfg_.certfile = nodeSSL["certfile"].as<std::string>();

        if (!nodeSSL["keyfile"].IsDefined()) {
            throw std::runtime_error("No SSL keyfile");
        }

        default_ssl_cfg_.keyfile = nodeSSL["keyfile"].as<std::string>();

        if (nodeSSL["password"].IsDefined()) {
            default_ssl_cfg_.password = nodeSSL["password"].as<std::string>();
        }

        if (nodeSSL["verify_mode"].IsDefined() &&
            nodeSSL["verify_mode"].as<std::string>() == "verify_peer") {
            default_ssl_cfg_.verify_mode = MQTT_SSL_VERIFY::PEER;

            if (nodeSSL["fail_if_no_peer_cert"].IsDefined()) {
                default_ssl_cfg_.fail_if_no_peer_cert =
                    nodeSSL["fail_if_no_peer_cert"].as<bool>();
            }

            if (!nodeSSL["cacertfile"].IsDefined()) {
                throw std::runtime_error("No SSL cacertfile");
            }

            default_ssl_cfg_.cacertfile =
                nodeSSL["cacertfile"].as<std::string>();
        }

        if (nodeSSL["dhparam"].IsDefined()) {
            default_ssl_cfg_.dhparam = nodeSSL["dhparam"].as<std::string>();
        }

        if (root["listeners"].IsDefined()) {
            parse_listeners(root["listeners"]);
        }

        if (root["server"].IsDefined()) {
            auto nodeServer = root["server"];

            if (nodeServer["protocol"].IsDefined()) {
                auto nodeProtocol = nodeServer["protocol"];

                if (nodeProtocol["connect_timeout"].IsDefined()) {
                    connect_timeout_ =
                        nodeProtocol["connect_timeout"].as<uint32_t>();
                }

                if (nodeProtocol["check_timeout_duration"].IsDefined()) {
                    check_timeout_duration_ =
                        nodeProtocol["check_timeout_duration"].as<uint32_t>();
                }

                if (nodeProtocol["check_waiting_map_duration"].IsDefined()) {
                    check_waiting_map_duration_ =
                        nodeProtocol["check_waiting_map_duration"]
                            .as<uint32_t>();
                }

                if (nodeProtocol["max_resend_count"].IsDefined()) {
                    max_resend_count_ =
                        nodeProtocol["max_resend_count"].as<uint32_t>();
                }

                if (nodeProtocol["resend_duration"].IsDefined()) {
                    resend_duration_ =
                        nodeProtocol["resend_duration"].as<uint32_t>();
                }

                if (nodeProtocol["max_waiting_time"].IsDefined()) {
                    max_waiting_time_ =
                        nodeProtocol["max_waiting_time"].as<uint32_t>();
                }

                if (nodeProtocol["auth"].IsDefined()) {
                    auth_ = nodeProtocol["auth"].as<bool>();

                    if (auth_ == true) {
                        if (nodeProtocol["credentials"].IsDefined()) {
                            for (const auto& credential :
                                 nodeProtocol["credentials"]) {
                                auto username =
                                    credential["username"].as<std::string>();
                                auto password =
                                    credential["password"].as<std::string>();

                                credentials_[username] = password;
                            }
                        }

                        if (nodeProtocol["acl"].IsDefined()) {
                            auto nodeAcl = nodeProtocol["acl"];
                            if (nodeAcl["enable"].IsDefined()) {
                                enable_ = nodeAcl["enable"].as<bool>();
                            }

                            if (enable_ && !nodeAcl["acl_file"].IsDefined()) {
                                throw std::runtime_error(
                                    "No ACL FILE configuration");
                            }

                            acl_file_ = nodeAcl["acl_file"].as<std::string>();

                            if (!acl_.load_acl(acl_file_)) {
                                throw std::runtime_error(
                                    "Faied to Load acl file: " + acl_file_);
                            }

                            if (nodeAcl["default"].IsDefined() &&
                                nodeAcl["default"].as<std::string>() ==
                                    "allow") {
                                default_ = true;
                            }
                        }
                    }
                }

                if (nodeProtocol["auto_subscribe_list"].IsDefined()) {
                    for (const auto& sub :
                         nodeProtocol["auto_subscribe_list"]) {
                        std::string topic = sub["topic"].as<std::string>();
                        uint8_t qos = sub["qos"].as<uint8_t>();

                        auto_subscribe_list_.emplace_back(std::move(topic),
                                                          qos);
                    }
                }
            }
        }

        if (root["log"].IsDefined()) {
            auto nodeLog = root["log"];

            if (nodeLog["name"].IsDefined()) {
                name_ = nodeLog["name"].as<std::string>();
            }

            if (nodeLog["max_rotate_size"].IsDefined()) {
                max_rotate_size_ = nodeLog["max_rotate_size"].as<uint32_t>();
            }

            if (nodeLog["max_rotate_count"].IsDefined()) {
                max_rotate_count_ = nodeLog["max_rotate_count"].as<uint32_t>();
            }

            if (nodeLog["thread_pool_qsize"].IsDefined()) {
                thread_pool_qsize_ =
                    nodeLog["thread_pool_qsize"].as<uint32_t>();
            }

            if (nodeLog["thread_count"].IsDefined()) {
                thread_count_ = nodeLog["thread_count"].as<uint32_t>();
            }
        }

    } catch (const std::exception& e) {
        std::printf("MqttConfig failed to parse file: [%s], error info: [%s]\n",
                    file_name.c_str(), e.what());

        return false;
    }

    return true;
}

bool MqttConfig::acl_check(const mqtt_acl_rule_t& rule) {
    auto state = acl_.check_acl(rule);
    if (state == MQTT_ACL_STATE::NONE) {
        return default_;
    }
    return state == MQTT_ACL_STATE::ALLOW;
}

void MqttConfig::parse_listeners(const YAML::Node& node) {
    for (const auto& nodeListener : node) {
        mqtt_listener_cfg_t cfg;

        cfg.ssl_cfg = mqtt_ssl_cfg_t{};

        cfg.port = nodeListener["port"].as<uint16_t>();

        std::string address = nodeListener["address"].as<std::string>();
        auto pos = address.find("://");
        if (pos == std::string::npos) {
            throw std::runtime_error("error format: listeners-address");
        }
        std::string protocol = address.substr(0, pos);
        cfg.address = address.substr(pos + 3);

        if (protocol == "mqtt") {
            cfg.proto = MQTT_PROTOCOL::MQTT;
        } else if (protocol == "mqtts") {
            cfg.proto = MQTT_PROTOCOL::MQTTS;
        } else if (protocol == "ws") {
            cfg.proto = MQTT_PROTOCOL::WS;
        } else if (protocol == "wss") {
            cfg.proto = MQTT_PROTOCOL::WSS;
        } else {
            throw std::runtime_error(
                "not supported protocol: listeners-address");
        }

        if (cfg.proto == MQTT_PROTOCOL::MQTTS ||
            cfg.proto == MQTT_PROTOCOL::WSS) {
            mqtt_ssl_cfg_t ssl_cfg;

            if (node["version"].IsDefined() &&
                node["version"].as<std::string>() == "tls1.3") {
                ssl_cfg.version = MQTT_SSL_VERSION::TLSv13;
            } else {
                ssl_cfg.version = default_ssl_cfg_.version;
            }

            if (!node["certfile"].IsDefined() ||
                node["certfile"].as<std::string>().empty()) {
                ssl_cfg.certfile = default_ssl_cfg_.certfile;
            } else {
                ssl_cfg.certfile = node["certfile"].as<std::string>();
            }

            if (!node["keyfile"].IsDefined() ||
                node["keyfile"].as<std::string>().empty()) {
                ssl_cfg.keyfile = default_ssl_cfg_.keyfile;
            } else {
                ssl_cfg.keyfile = node["keyfile"].as<std::string>();
            }

            if (node["password"].IsDefined()) {
                ssl_cfg.password = node["password"].as<std::string>();
            } else {
                ssl_cfg.password = default_ssl_cfg_.password;
            }

            if (node["verify_mode"].IsDefined()) {
                if (node["verify_mode"].as<std::string>() == "verfy_peer") {
                    ssl_cfg.verify_mode = MQTT_SSL_VERIFY::PEER;
                } else {
                    ssl_cfg.verify_mode = MQTT_SSL_VERIFY::NONE;
                }
            } else {
                ssl_cfg.verify_mode = default_ssl_cfg_.verify_mode;
            }

            if (ssl_cfg.verify_mode == MQTT_SSL_VERIFY::PEER) {
                if (node["fail_if_no_peer_cert"].IsDefined()) {
                    ssl_cfg.fail_if_no_peer_cert =
                        node["fail_if_no_peer_cert"].as<bool>();
                } else {
                    ssl_cfg.fail_if_no_peer_cert =
                        default_ssl_cfg_.fail_if_no_peer_cert;
                }

                if (!node["cacertfile"].IsDefined()) {
                    ssl_cfg.cacertfile = default_ssl_cfg_.cacertfile;
                } else {
                    ssl_cfg.cacertfile = node["cacertfile"].as<std::string>();
                }
            }

            if (node["dhparam"].IsDefined()) {
                ssl_cfg.dhparam = node["dhparam"].as<std::string>();
            } else {
                ssl_cfg.dhparam = default_ssl_cfg_.dhparam;
            }

            cfg.ssl_cfg = std::move(ssl_cfg);
        }

        listeners_.emplace_back(std::move(cfg));
    }
}