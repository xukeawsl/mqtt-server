#include "MqttServer.h"

MqttServer::MqttServer(const std::string& host, uint16_t port)
    : io_context(1),
      signals(io_context),
      acceptor(io_context),
      listen_endpoint(asio::ip::make_address(host), port) {}

MqttServer::~MqttServer() {
    SPDLOG_INFO("Mqtt Server Stopped");
    spdlog::shutdown();
}

void MqttServer::run() noexcept {
    try {
        init();

        asio::co_spawn(acceptor.get_executor(), handle_accept(),
                       asio::detached);
        
        SPDLOG_INFO("Mqtt Server Start");
        SPDLOG_INFO("Mqtt Server Listening on {}", listen_endpoint);
        SPDLOG_INFO("Mqtt Server Listening Adress Type : {}",
                    listen_endpoint.address().is_v4() ? "IPv4" : "IPv6");

        io_context.run();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Mqtt Server Failed to Start : ERR_MSG = [{}])",
                     std::string(e.what()));
    }
}

void MqttServer::init() {
    signals.add(SIGINT);
    signals.add(SIGTERM);
#if defined(SIGQUIT)
    signals.add(SIGQUIT);
#endif
    signals.async_wait(std::bind(&MqttServer::stop, this));

    acceptor.open(listen_endpoint.protocol());
    acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(listen_endpoint);
    acceptor.listen();
}

void MqttServer::stop() { io_context.stop(); }

asio::awaitable<void> MqttServer::handle_accept() {
    try {
        MqttBroker broker;
        for (;;) {
            std::make_shared<MqttSession>(
                co_await acceptor.async_accept(asio::use_awaitable), broker)
                ->start();
        }
    } catch (const std::exception& e) {
    }
}