#include "MqttServer.h"

MqttServer::MqttServer(const std::string& host, uint16_t port)
    : io_context(1),
      signals(io_context),
      acceptor(io_context),
#ifdef MQ_WITH_TLS
      ssl_context(asio::ssl::context::tlsv12_server),
#endif
      listen_endpoint(asio::ip::make_address(host), port) {
}

MqttServer::~MqttServer() {
    SPDLOG_INFO("Mqtt Server Stopped");
    spdlog::shutdown();
}

void MqttServer::run() noexcept {
    try {
        init();

        SPDLOG_INFO("Mqtt Server Start");
        SPDLOG_INFO("Mqtt Server Listening on {}",
                    convert::format_address(listen_endpoint));
        SPDLOG_INFO("Mqtt Server Listening Adress Type : {}",
                    listen_endpoint.address().is_v4() ? "IPv4" : "IPv6");
#ifdef MQ_WITH_TLS
        // 支持 TLS1.2 和 TLS1.3 版本
        if (MqttConfig::getInstance()->version() ==
            MqttConfig::VERSION::TLSv13) {
            ssl_context = asio::ssl::context(asio::ssl::context::tlsv13_server);
            SPDLOG_INFO("Mqtt Server SSL/TLS Version is TLSv1.3");
        } else {
            SPDLOG_INFO("Mqtt Server SSL/TLS Version is TLSv1.2");
        }

        auto ssl_options =
            asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 |
            asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1;

        std::string dhparam_file = MqttConfig::getInstance()->dhparam();
        if (!dhparam_file.empty()) {
            ssl_options |= asio::ssl::context::single_dh_use;
            ssl_context.use_tmp_dh_file(dhparam_file);
        }

        ssl_context.set_options(asio::ssl::context::default_workarounds |
                                asio::ssl::context::no_sslv2);

        auto mode = asio::ssl::verify_none;

        if (MqttConfig::getInstance()->verify_mode() ==
            MqttConfig::SSL_VERIFY::PEER) {
            mode = asio::ssl::verify_peer;

            if (MqttConfig::getInstance()->fail_if_no_peer_cert()) {
                mode |= asio::ssl::verify_fail_if_no_peer_cert;
            }

            //双向认证必须提供 CA 证书
            ssl_context.load_verify_file(
                MqttConfig::getInstance()->cacertfile());

            SPDLOG_INFO("Mqtt Server SSL/TLS Verify Mode is verify_peer");
        } else {
            SPDLOG_INFO("Mqtt Server SSL/TLS Verify Mode is verify_none");
        }

        ssl_context.set_verify_mode(mode);

        // 设置回调必须在设置私钥之前
        std::string password = MqttConfig::getInstance()->password();
        if (!password.empty()) {
            ssl_context.set_password_callback(
                [pw = std::move(password)](
                    std::size_t,
                    asio::ssl::context_base::password_purpose) -> std::string {
                    return pw;
                });
        }

        std::string server_crt = MqttConfig::getInstance()->certfile();
        std::string server_key = MqttConfig::getInstance()->keyfile();

        SPDLOG_INFO("Mqtt Server certfile is {}", server_crt);
        SPDLOG_INFO("Mqtt Server private key file is {}", server_key);

        ssl_context.use_certificate_chain_file(server_crt);
        ssl_context.use_private_key_file(server_key, asio::ssl::context::pem);

#endif
        asio::co_spawn(acceptor.get_executor(), handle_accept(),
                       asio::detached);

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
#ifdef MQ_WITH_TLS
            asio::ssl::stream<asio::ip::tcp::socket> ssl_socket(io_context,
                                                                ssl_context);
            co_await acceptor.async_accept(ssl_socket.next_layer(),
                                           asio::use_awaitable);
            std::make_shared<MqttSession>(std::move(ssl_socket), broker)
                ->start();
#else
            std::make_shared<MqttSession>(
                co_await acceptor.async_accept(asio::use_awaitable), broker)
                ->start();
#endif
        }
    } catch (const std::exception& e) {
    }
}
