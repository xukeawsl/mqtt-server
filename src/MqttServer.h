#pragma once

#include "MqttCommon.h"
#include "MqttBroker.h"

class MqttServer {
public:
    MqttServer();

    ~MqttServer();

    void run() noexcept;

private:
    void init();

    void stop();

    asio::awaitable<void> handle_accept(asio::ip::tcp::acceptor acceptor, const mqtt_listener_cfg_t& cfg);

private:
    asio::io_context io_context;
    asio::signal_set signals;
#ifdef MQ_WITH_TLS
    MqttBroker<asio::ip::tcp::socket, asio::ssl::stream<asio::ip::tcp::socket>> broker;
#else
    MqttBroker<asio::ip::tcp::socket> broker;
#endif
};