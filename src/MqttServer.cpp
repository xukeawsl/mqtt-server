#include "MqttServer.h"

#include "MqttSession.h"

MqttServer::MqttServer() : io_context(1), signals(io_context) {}

MqttServer::~MqttServer() {}

void MqttServer::run() noexcept {
    try {
        init();

        SPDLOG_INFO("Mqtt Server started successfully");

        io_context.run();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Mqtt Server failed to start : ERR_MSG = [{}])",
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

    for (const auto& cfg : MqttConfig::getInstance()->listeners()) {
        if (cfg.proto == MQTT_PROTOCOL::MQTTS ||
            cfg.proto == MQTT_PROTOCOL::WSS) {
#ifndef MQ_WITH_TLS
            continue;
#endif
        }

        auto listen_endpoint = asio::ip::tcp::endpoint(
            asio::ip::make_address(cfg.address), cfg.port);

        asio::ip::tcp::acceptor acceptor(this->io_context);
        acceptor.open(listen_endpoint.protocol());
        acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor.bind(listen_endpoint);
        acceptor.listen();

        asio::co_spawn(acceptor.get_executor(),
                       handle_accept(std::move(acceptor), cfg), asio::detached);
    }
}

void MqttServer::stop() {
    io_context.stop();
    SPDLOG_INFO("Mqtt Server stopped successfully");
}

asio::awaitable<void> MqttServer::handle_accept(
    asio::ip::tcp::acceptor acceptor, const mqtt_listener_cfg_t& cfg) {
    bool is_websocket =
        (cfg.proto == MQTT_PROTOCOL::WS || cfg.proto == MQTT_PROTOCOL::WSS);

#ifdef MQ_WITH_TLS
    if (cfg.proto == MQTT_PROTOCOL::MQTTS || cfg.proto == MQTT_PROTOCOL::WSS) {
        asio::ssl::context ssl_context(asio::ssl::context::tlsv12_server);

        if (cfg.ssl_cfg.version == MQTT_SSL_VERSION::TLSv13) {
            ssl_context = asio::ssl::context(asio::ssl::context::tlsv13_server);
        }

        auto ssl_options =
            asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 |
            asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1;

        if (!cfg.ssl_cfg.dhparam.empty()) {
            ssl_options |= asio::ssl::context::single_dh_use;
            ssl_context.use_tmp_dh_file(cfg.ssl_cfg.dhparam);
        }

        ssl_context.set_options(asio::ssl::context::default_workarounds |
                                asio::ssl::context::no_sslv2);

        auto mode = asio::ssl::verify_none;

        if (cfg.ssl_cfg.verify_mode == MQTT_SSL_VERIFY::PEER) {
            mode = asio::ssl::verify_peer;

            if (cfg.ssl_cfg.fail_if_no_peer_cert) {
                mode |= asio::ssl::verify_fail_if_no_peer_cert;
            }

            // 双向认证必须提供 CA 证书
            ssl_context.load_verify_file(cfg.ssl_cfg.cacertfile);
        }

        ssl_context.set_verify_mode(mode);

        // 设置回调必须在设置私钥之前
        std::string password = cfg.ssl_cfg.password;
        if (!password.empty()) {
            ssl_context.set_password_callback(
                [pw = std::move(password)](
                    std::size_t,
                    asio::ssl::context_base::password_purpose) -> std::string {
                    return pw;
                });
        }

        std::string server_crt = cfg.ssl_cfg.certfile;
        std::string server_key = cfg.ssl_cfg.keyfile;

        ssl_context.use_certificate_chain_file(server_crt);
        ssl_context.use_private_key_file(server_key, asio::ssl::context::pem);

        for (;;) {
            asio::ssl::stream<asio::ip::tcp::socket> ssl_socket(
                this->io_context, ssl_context);
            co_await acceptor.async_accept(ssl_socket.next_layer(),
                                           asio::use_awaitable);
            std::make_shared<
                MqttSession<asio::ssl::stream<asio::ip::tcp::socket>>>(
                std::move(ssl_socket), is_websocket, broker)
                ->start();
        }

    } else {
        for (;;) {
            std::make_shared<MqttSession<asio::ip::tcp::socket>>(
                co_await acceptor.async_accept(asio::use_awaitable),
                is_websocket, broker)
                ->start();
        }
    }
#else
    for (;;) {
        std::make_shared<MqttSession<asio::ip::tcp::socket>>(
            co_await acceptor.async_accept(asio::use_awaitable), is_websocket,
            broker)
            ->start();
    }
#endif

    co_return;
}
