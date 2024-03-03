#include "MqttConfig.h"

#include "yaml-cpp/yaml.h"

MqttConfig* MqttConfig::getInstance() {
    static MqttConfig config;
    return &config;
}

MqttConfig::MqttConfig() {
    address_ = "0.0.0.0";
    port_ = 1883;
    connect_timeout_ = 60;
    check_timeout_duration_ = 1;
    check_waiting_map_duration_ = 1;
    max_resend_count_ = 3;
    resend_duration_ = 60;
    max_waiting_time_ = 60;
    auth_ = false;
    name_ = "mqtt-server.log";
    max_rotate_size_ = 1024 * 1024;
    thread_pool_qsize_ = 8192;
    thread_count_ = 1;
}

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
                        if (nodeProtocol["credentials"].IsDefined() &&
                            nodeProtocol["credentials"].IsSequence()) {
                            for (const auto& credential :
                                 nodeProtocol["credentials"]) {
                                auto username =
                                    credential["username"].as<std::string>();
                                auto password =
                                    credential["password"].as<std::string>();

                                credentials_[username] = password;
                            }
                        }
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