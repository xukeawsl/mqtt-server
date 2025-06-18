#include "MqttConfig.h"
#include "MqttExposer.h"
#include "MqttServer.h"

int main(int argc, char* argv[]) {
    if (!MqttConfig::getInstance()->parse("../config.yml")) {
        return EXIT_FAILURE;
    }

    if (!MqttLogger::getInstance()->init()) {
        return EXIT_FAILURE;
    }

    if (!MqttExposer::getInstance()->run()) {
        SPDLOG_ERROR("Failed to start MQTT Exposer");
        return EXIT_FAILURE;
    }

    MqttServer server;
    server.run();

    MqttExposer::getInstance()->stop();

    MqttLogger::getInstance()->shutdown();

    return EXIT_SUCCESS;
}
