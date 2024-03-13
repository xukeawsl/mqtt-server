#include "MqttConfig.h"
#include "MqttServer.h"

int main(int argc, char* argv[]) {
    if (!MqttConfig::getInstance()->parse("../config.yml")) {
        return EXIT_FAILURE;
    }

    if (!MqttLogger::getInstance()->init(
            MqttConfig::getInstance()->name(),
            MqttConfig::getInstance()->max_rotate_size(),
            MqttConfig::getInstance()->max_rotate_count())) {
        return EXIT_FAILURE;
    }

    MqttServer server(MqttConfig::getInstance()->address(),
                      MqttConfig::getInstance()->port());
    server.run();

    return EXIT_SUCCESS;
}