#include "MqttConfig.h"

#include "yaml-cpp/yaml.h"

MqttConfig* MqttConfig::getInstance() {
    static MqttConfig config;
    return &config;
}

MqttConfig::MqttConfig()
    : address_("0.0.0.0"),
      port_(1883),
      connect_timeout_(10),
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
      version_(VERSION::TLSv12),
      verify_mode_(SSL_VERIFY::NONE),
      fail_if_no_peer_cert_(false),
      enable_(false),
      default_(false) {}

bool MqttConfig::parse(const std::string& file_name) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(file_name);

        if (root["server"].IsDefined()) {
            auto nodeServer = root["server"];

            if (nodeServer["address"].IsDefined()) {
                address_ = nodeServer["address"].as<std::string>();
            }

            if (nodeServer["port"].IsDefined()) {
                port_ = nodeServer["port"].as<uint16_t>();
            }

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
                                throw std::runtime_error("No ACL FILE configuration");
                            }

                            acl_file_ = nodeAcl["acl_file"].as<std::string>();

                            if (!acl_.load_acl(acl_file_)) {
                                throw std::runtime_error("Faied to Load acl file: " + acl_file_);
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

#ifdef MQ_WITH_TLS
        if (!root["ssl"].IsDefined()) {
            throw std::runtime_error("No SSL configuration");
        }

        auto nodeSSL = root["ssl"];

        if (nodeSSL["version"].IsDefined() &&
            nodeSSL["version"].as<std::string>() == "tls1.3") {
            version_ = VERSION::TLSv13;
        }

        if (!nodeSSL["address"].IsDefined()) {
            throw std::runtime_error("No SSL listen address");
        }

        address_ = nodeSSL["address"].as<std::string>();

        if (!nodeSSL["port"].IsDefined()) {
            throw std::runtime_error("No SSL listen port");
        }

        port_ = nodeSSL["port"].as<std::uint16_t>();

        if (!nodeSSL["certfile"].IsDefined()) {
            throw std::runtime_error("No SSL certfile");
        }

        certfile_ = nodeSSL["certfile"].as<std::string>();

        if (!nodeSSL["keyfile"].IsDefined()) {
            throw std::runtime_error("No SSL keyfile");
        }

        keyfile_ = nodeSSL["keyfile"].as<std::string>();

        if (nodeSSL["password"].IsDefined()) {
            password_ = nodeSSL["password"].as<std::string>();
        }

        if (!nodeSSL["verify_mode"].IsDefined()) {
            throw std::runtime_error("No SSL verify_mode");
        }

        if (nodeSSL["verify_mode"].as<std::string>() == "verify_peer") {
            verify_mode_ = SSL_VERIFY::PEER;

            if (nodeSSL["fail_if_no_peer_cert"].IsDefined()) {
                fail_if_no_peer_cert_ =
                    nodeSSL["fail_if_no_peer_cert"].as<bool>();
            }

            if (!nodeSSL["cacertfile"].IsDefined()) {
                throw std::runtime_error("No SSL cacertfile");
            }

            cacertfile_ = nodeSSL["cacertfile"].as<std::string>();
        }

        if (nodeSSL["dhparam"].IsDefined()) {
            dhparam_ = nodeSSL["dhparam"].as<std::string>();
        }
#endif

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