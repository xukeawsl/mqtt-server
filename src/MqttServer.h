#pragma once

#include "MqttBroker.h"
#include "MqttSession.h"

class MqttServer {
public:
    MqttServer(const std::string& host, uint16_t port);

    ~MqttServer();

    void run() noexcept;

private:
    void init();

    void stop();

    asio::awaitable<void> handle_accept();

private:
    asio::io_context io_context;
    asio::signal_set signals;
    asio::ip::tcp::acceptor acceptor;
    asio::ip::tcp::endpoint listen_endpoint;
#ifdef MQ_WITH_TLS
    asio::ssl::context ssl_context;
#endif
};