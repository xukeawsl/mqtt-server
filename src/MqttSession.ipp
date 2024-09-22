#include "MqttSession.h"

using namespace std::string_view_literals;

static bool tolower_equal(std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](char a, char b) { return tolower(a) == tolower(b); });
}

static std::string_view trim_sv(std::string_view v) {
    v.remove_prefix((std::min)(v.find_first_not_of(" "), v.size()));
    v.remove_suffix(
        (std::min)(v.size() - v.find_last_not_of(" ") - 1, v.size()));
    return v;
}

template <typename SocketType>
std::unordered_set<std::string> MqttSession<SocketType>::active_sub_set;

template <typename SocketType>
MqttSession<SocketType>::MqttSession(
    SocketType socket, bool is_websocket,
#ifdef MQ_WITH_TLS
    MqttBroker<asio::ip::tcp::socket, asio::ssl::stream<asio::ip::tcp::socket>>&
        mqtt_broker
#else
    MqttBroker<asio::ip::tcp::socket>& mqtt_broker
#endif
    )
    : socket(std::move(socket)),
      is_websocket(is_websocket),
      broker(mqtt_broker),
      cond_timer(socket.get_executor()),
      keep_alive_timer(socket.get_executor()),
      check_timer(socket.get_executor()),
      write_lock(socket.get_executor(), 1),
      complete_connect(false),
      rc(MQTT_RC_CODE::ERR_SUCCESS),
      command(0),
      pos(0),
      remaining_length(0) {
    this->cond_timer.expires_at(std::chrono::steady_clock::time_point::max());
    this->keep_alive_timer.expires_at(
        std::chrono::steady_clock::time_point::max());
    this->check_timer.expires_at(std::chrono::steady_clock::time_point::max());
}

template <typename SocketType>
MqttSession<SocketType>::~MqttSession() {}

template <typename SocketType>
void MqttSession<SocketType>::start() {
#ifdef MQ_WITH_TLS
    if constexpr (std::is_same_v<SocketType,
                                 asio::ssl::stream<asio::ip::tcp::socket>>) {
        asio::co_spawn(
            socket.get_executor(),
            [self = this->shared_from_this()] {
                return self->handle_ssl_handshake();
            },
            asio::detached);
    } else {
        handle_session();
    }
#else
    handle_session();
#endif
}

template <typename SocketType>
void MqttSession<SocketType>::handle_session() {
    // CONNECT 阶段的超时时间
    this->session_state.keep_alive =
        MqttConfig::getInstance()->connect_timeout();

    asio::co_spawn(
        socket.get_executor(),
        [self = this->shared_from_this()] { return self->handle_packet(); },
        asio::detached);

    asio::co_spawn(
        socket.get_executor(),
        [self = this->shared_from_this()] { return self->handle_keep_alive(); },
        asio::detached);
}

#ifdef MQ_WITH_TLS
template <typename SocketType>
asio::awaitable<void> MqttSession<SocketType>::handle_ssl_handshake() {
    try {
        co_await socket.async_handshake(asio::ssl::stream_base::server,
                                        asio::use_awaitable);
        handle_session();
    } catch (std::exception& e) {
        SPDLOG_ERROR("handshake failed : [{}]", e.what());
        disconnect();
    }
}
#endif

template <typename SocketType>
std::string MqttSession<SocketType>::get_session_id() {
    return this->client_id;
}

template <typename SocketType>
void MqttSession<SocketType>::disconnect() {
    asio::error_code ignored_ec;
#ifdef MQ_WITH_TLS
    if constexpr (std::is_same_v<SocketType,
                                 asio::ssl::stream<asio::ip::tcp::socket>>) {
        this->socket.shutdown(ignored_ec);
        this->socket.next_layer().close(ignored_ec);
    } else {
        this->socket.close(ignored_ec);
    }
#else
    this->socket.close(ignored_ec);
#endif
    this->cond_timer.cancel(ignored_ec);
    this->keep_alive_timer.cancel(ignored_ec);
    this->check_timer.cancel(ignored_ec);
    this->write_lock.cancel();
}

template <typename SocketType>
void MqttSession<SocketType>::init_buffer() {
    this->rc = MQTT_RC_CODE::ERR_SUCCESS;
    this->pos = 0;
    this->command = 0;
    this->remaining_length = 0;
    this->payload.clear();
    this->payload.shrink_to_fit();
}

template <typename SocketType>
void MqttSession<SocketType>::flush_deadline() {
    if (this->session_state.keep_alive > 0) {
        uint16_t keep_alive = this->session_state.keep_alive * 3 / 2;

        this->deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(keep_alive);
    }
}

template <typename SocketType>
void MqttSession<SocketType>::handle_error_code() {
    disconnect();

    // 会话清理
    if (this->complete_connect && this->session_state.clean_session) {
        MqttSession<SocketType>::active_sub_set.erase(this->client_id);
        this->broker.leave(this->client_id);
    }

    // 如果存在遗嘱消息就发送
    if (this->complete_connect &&
        this->session_state.will_topic.topic_name->length()) {
        // 如果是保留的需要添加到保留消息集合中
        if (this->session_state.will_topic.retain) {
            this->broker.add_retain(this->session_state.will_topic);
        }

        SPDLOG_DEBUG("send will topic [{}]",
                     *this->session_state.will_topic.topic_name);

        // 将保留消息标志置零后分发
        this->session_state.will_topic.retain = 0;

        this->broker.dispatch_will(this->session_state.will_topic,
                                   this->client_id);
    }

    switch (this->rc) {
        case MQTT_RC_CODE::ERR_SUCCESS:
        case MQTT_RC_CODE::ERR_SUCCESS_DISCONNECT: {
            SPDLOG_DEBUG("Normal disconnection");
            break;
        }
        case MQTT_RC_CODE::ERR_NO_CONN: {
            SPDLOG_WARN("Abnormal disconnection");
            break;
        }
        case MQTT_RC_CODE::ERR_DUP_CONNECT: {
            SPDLOG_ERROR("Duplicate CONNECT requests");
            break;
        }
        case MQTT_RC_CODE::ERR_NOT_CONNECT: {
            SPDLOG_ERROR("The CONNECT message has not been sent yet");
            break;
        }
        case MQTT_RC_CODE::ERR_COMMAND: {
            SPDLOG_ERROR("Wrong command type");
            break;
        }
        case MQTT_RC_CODE::ERR_COMMAND_RESERVED: {
            SPDLOG_ERROR("Wrong command reserved");
            break;
        }
        case MQTT_RC_CODE::ERR_REMAINING_LENGTH: {
            SPDLOG_ERROR("Wrong remaining length");
            break;
        }
        case MQTT_RC_CODE::ERR_PROTOCOL: {
            SPDLOG_ERROR("Incorrect or unsupported protocol type");
            break;
        }
        case MQTT_RC_CODE::ERR_MALFORMED_UTF8: {
            SPDLOG_ERROR("Wrong mqtt utf8 string formation");
            break;
        }
        case MQTT_RC_CODE::ERR_MALFORMED_PACKET: {
            SPDLOG_ERROR("Wrong mqtt packet formation");
            break;
        }
        case MQTT_RC_CODE::ERR_NOT_SUPPORTED: {
            SPDLOG_ERROR("Not Supported");
            break;
        }
        case MQTT_RC_CODE::ERR_PAYLOAD_SIZE: {
            SPDLOG_ERROR("Wrong mqtt payload length");
            break;
        }
        case MQTT_RC_CODE::ERR_STR_LENGTH_UTF8: {
            SPDLOG_ERROR("Wrong mqtt utf8 string length");
            break;
        }
        case MQTT_RC_CODE::ERR_SUB_TOPIC_NAME: {
            SPDLOG_ERROR("Wrong mqtt subscribe topic name");
            break;
        }
        case MQTT_RC_CODE::ERR_PUB_TOPIC_NAME: {
            SPDLOG_ERROR("Wrong mqtt publish topic name");
            break;
        }
        case MQTT_RC_CODE::ERR_BAD_USERNAME_PASSWORD: {
            SPDLOG_ERROR("Wrong mqtt username/password");
            break;
        }
        case MQTT_RC_CODE::ERR_BAD_CLIENT_ID: {
            SPDLOG_ERROR("Wrong mqtt client id");
            break;
        }
        case MQTT_RC_CODE::ERR_REFUSED_NOT_AUTHORIZED: {
            SPDLOG_ERROR("Wrong authentication");
            break;
        }
        default: {
            SPDLOG_ERROR("Other error");
        }
    }
}

template <typename SocketType>
uint16_t MqttSession<SocketType>::gen_packet_id() {
    do {
        this->session_state.packet_id_gen++;

        // packet id must > 0
        if (this->session_state.packet_id_gen == 0) {
            this->session_state.packet_id_gen++;
        }
    } while (this->session_state.waiting_map.contains(
        this->session_state.packet_id_gen));

    return this->session_state.packet_id_gen;
}

template <typename SocketType>
asio::awaitable<void> MqttSession<SocketType>::handle_keep_alive() {
    auto check_duration = std::chrono::seconds(
        MqttConfig::getInstance()->check_timeout_duration());

    // 每到一次间隔时间检查一下是否超时
    while (this->is_open()) {
        this->keep_alive_timer.expires_after(check_duration);

        co_await this->keep_alive_timer.async_wait(asio::use_awaitable);

        if (this->deadline <= std::chrono::steady_clock::now()) {
            disconnect();
        }
    }
}

template <typename SocketType>
void MqttSession<SocketType>::move_session_state(
    std::shared_ptr<MqttSession<SocketType>> new_session) {
    // 连接完成标志置为未完成, 这样连接断开后不会再去调用 leave 删除会话
    // 也不会发送遗嘱消息
    this->complete_connect = false;

    // 关闭旧会话的连接
    disconnect();

    // 恢复会话状态
    if (new_session->session_state.clean_session == 0) {
        new_session->session_state.inflight_queue =
            std::move(this->session_state.inflight_queue);
        new_session->session_state.sub_topic_map =
            std::move(this->session_state.sub_topic_map);
        new_session->session_state.waiting_map =
            std::move(this->session_state.waiting_map);
    }
}

template <typename SocketType>
void MqttSession<SocketType>::push_packet(const mqtt_packet_t& packet) {
    this->session_state.inflight_queue.emplace(packet);
    SPDLOG_DEBUG("push packet: topic_name = [{}], payload = [{}]",
                 *packet.topic_name, *packet.payload);

    // 通知处理线程读取消息
    this->cond_timer.cancel_one();
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_byte(
    uint8_t* addr, bool read_payload) {
    asio::error_code ec;

    if (read_payload) {
        if (this->pos + 1 > this->remaining_length) {
            co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
        }

        *addr = this->payload[this->pos];
        this->pos += 1;

        co_return MQTT_RC_CODE::ERR_SUCCESS;
    }

    co_await async_read(this->socket, asio::buffer(addr, 1),
                        asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    co_return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_uint16(
    uint16_t* addr, bool read_payload) {
    asio::error_code ec;
    uint8_t msb, lsb;

    if (read_payload) {
        if (this->pos + 2 > this->remaining_length) {
            co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
        }

        msb = this->payload[this->pos];
        lsb = this->payload[this->pos + 1];

        *addr = static_cast<uint16_t>((msb << 8) + lsb);

        this->pos += 2;

        co_return MQTT_RC_CODE::ERR_SUCCESS;
    }

    std::array<asio::mutable_buffer, 2> buf = {
        {asio::buffer(&msb, sizeof(uint8_t)),
         asio::buffer(&lsb, sizeof(uint8_t))}};

    co_await async_read(this->socket, buf,
                        asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    *addr = static_cast<uint16_t>((msb << 8) + lsb);

    co_return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_bytes_to_buf(
    std::string& bytes, uint32_t n, bool read_payload) {
    asio::error_code ec;

    if (n == 0) {
        co_return MQTT_RC_CODE::ERR_SUCCESS;
    }

    bytes.resize(n);

    if (read_payload) {
        if (this->pos + n > this->remaining_length) {
            co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
        }

        std::memcpy(bytes.data(), this->payload.data() + this->pos, n);

        this->pos += n;

        co_return MQTT_RC_CODE::ERR_SUCCESS;
    }

    co_await async_read(this->socket,
                        asio::buffer(bytes.data(), bytes.length()),
                        asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    co_return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE>
MqttSession<SocketType>::read_uint16_header_length_bytes(std::string& bytes,
                                                         bool read_payload) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    uint16_t slen;

    rc = co_await read_uint16(&slen, read_payload);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (slen == 0) {
        co_return rc;
    }

    bytes.resize(slen);

    if (read_payload) {
        if (this->pos + slen > this->remaining_length) {
            co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
        }

        std::memcpy(bytes.data(), this->payload.data() + this->pos, slen);

        this->pos += slen;
        co_return MQTT_RC_CODE::ERR_SUCCESS;
    }

    co_await async_read(this->socket,
                        asio::buffer(bytes.data(), bytes.length()),
                        asio::redirect_error(asio::use_awaitable, ec));

    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    co_return rc;
}

template <typename SocketType>
MQTT_RC_CODE MqttSession<SocketType>::check_validate_utf8(
    const std::string& ustr) {
    uint32_t len = ustr.length();
    uint32_t i, j, codelen, codepoint;

    if (len > 65536) return MQTT_RC_CODE::ERR_STR_LENGTH_UTF8;

    for (i = 0; i < len; i++) {
        if (ustr[i] == 0) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        } else if (ustr[i] <= 0x7f) {
            codelen = 1;
            codepoint = ustr[i];
        } else if ((ustr[i] & 0xE0) == 0xC0) {
            if (ustr[i] == 0xC0 || ustr[i] == 0xC1) {
                return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
            }
            codelen = 2;
            codepoint = (ustr[i] & 0x1F);
        } else if ((ustr[i] & 0xF0) == 0xE0) {
            codelen = 3;
            codepoint = (ustr[i] & 0x0F);
        } else if ((ustr[i] & 0xF8) == 0xF0) {
            if (ustr[i] > 0xF4) {
                return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
            }
            codelen = 4;
            codepoint = (ustr[i] & 0x07);
        } else {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        }

        for (j = 0; j < codelen - 1; j++) {
            if ((ustr[++i] & 0xC0) != 0x80) {
                return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
            }
            codepoint = (codepoint << 6) | (ustr[i] & 0x3F);
        }

        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        }

        if (codelen == 3 && codepoint < 0x0800) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        } else if (codelen == 4 &&
                   (codepoint < 0x10000 || codepoint > 0x10FFFF)) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        }

        if (codepoint >= 0xFDD0 && codepoint <= 0xFDEF) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        }
        if ((codepoint & 0xFFFF) == 0xFFFE || (codepoint & 0xFFFF) == 0xFFFF) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        }

        if (codepoint <= 0x001F ||
            (codepoint >= 0x007F && codepoint <= 0x009F)) {
            return MQTT_RC_CODE::ERR_MALFORMED_UTF8;
        }
    }

    return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_utf8_string(
    std::string& str, bool read_payload) {
    MQTT_RC_CODE rc;

    rc = co_await read_uint16_header_length_bytes(str, read_payload);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = check_validate_utf8(str);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    co_return rc;
}

template <typename SocketType>
MQTT_RC_CODE MqttSession<SocketType>::check_sub_topic(
    const std::string& topic_name) {
    if (topic_name.empty()) {
        return MQTT_RC_CODE::ERR_SUB_TOPIC_NAME;
    }

    uint32_t slen = topic_name.length();

    if (slen > 65535) {
        return MQTT_RC_CODE::ERR_STR_LENGTH_UTF8;
    }

    for (uint32_t i = 0; i < slen; i++) {
        if (topic_name[i] == '+') {
            // 只能单独占一个主题层级
            if ((i == 0 || topic_name[i - 1] == '/') &&
                (i == slen - 1 || topic_name[i + 1] == '/')) {
            } else {
                return MQTT_RC_CODE::ERR_SUB_TOPIC_NAME;
            }

        } else if (topic_name[i] == '#') {
            // 只能是最后一个且只有当没有其它字符时前面才能没有 '/'
            if (i == slen - 1 && (i == 0 || topic_name[i - 1] == '/')) {
            } else {
                return MQTT_RC_CODE::ERR_SUB_TOPIC_NAME;
            }
        }
    }

    return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
MQTT_RC_CODE MqttSession<SocketType>::check_pub_topic(
    const std::string& topic_name) {
    if (topic_name.length() > 65535) {
        return MQTT_RC_CODE::ERR_STR_LENGTH_UTF8;
    }

    // 不能向包含通配符 #、+ 的主题发布消息
    for (auto ch : topic_name) {
        if (ch == '+' || ch == '#') {
            return MQTT_RC_CODE::ERR_PUB_TOPIC_NAME;
        }
    }

    return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_will_packet(
    mqtt_packet_t& packet, bool read_payload) {
    MQTT_RC_CODE rc;
    std::string will_topic_name;
    std::string will_payload;
    uint16_t will_payloadlen;

    rc = co_await read_utf8_string(will_topic_name, read_payload);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = check_sub_topic(will_topic_name);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = co_await read_uint16(&will_payloadlen, read_payload);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (will_payloadlen > 0) {
        rc = co_await read_bytes_to_buf(will_payload, will_payloadlen,
                                        read_payload);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    }

    packet.topic_name =
        std::make_shared<const std::string>(std::move(will_topic_name));
    packet.payload =
        std::make_shared<const std::string>(std::move(will_payload));

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_remaining_length() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    uint8_t byte;

    for (uint32_t remaining_count = 0, remaining_mult = 1; remaining_count < 4;
         remaining_count++, remaining_mult <<= 7) {
        rc = co_await read_byte(&byte, false);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        this->remaining_length += (byte & 0x7F) * remaining_mult;

        if (!(byte & 0x80)) break;
    }

    if (byte & 0x80) {
        rc = MQTT_RC_CODE::ERR_REMAINING_LENGTH;
    }

    SPDLOG_DEBUG("remaning length = [{}]", this->remaining_length);

    co_return rc;
}

template <typename SocketType>
MQTT_RC_CODE MqttSession<SocketType>::check_command() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    switch (this->command & 0xF0) {
        case MQTT_CMD::CONNECT:
        case MQTT_CMD::CONNACK:
        case MQTT_CMD::PUBACK:
        case MQTT_CMD::PUBREC:
        case MQTT_CMD::PUBCOMP:
        case MQTT_CMD::SUBACK:
        case MQTT_CMD::UNSUBACK:
        case MQTT_CMD::PINGREQ:
        case MQTT_CMD::PINGRESP:
        case MQTT_CMD::DISCONNECT: {
            if ((this->command & 0x0F) != 0x00) {
                rc = MQTT_RC_CODE::ERR_COMMAND_RESERVED;
            }
            break;
        }
        case MQTT_CMD::PUBLISH: {
            break;
        }
        case MQTT_CMD::PUBREL:
        case MQTT_CMD::SUBSCRIBE:
        case MQTT_CMD::UNSUBSCRIBE: {
            if ((this->command & 0x0F) != 0x02) {
                rc = MQTT_RC_CODE::ERR_COMMAND_RESERVED;
            }
            break;
        }
        default: {
            rc = MQTT_RC_CODE::ERR_COMMAND;
        }
    }

    SPDLOG_DEBUG("commmand = [X'{:02X}'] rc = [X'{:02X}']",
                 static_cast<uint16_t>(this->command),
                 static_cast<uint16_t>(rc));

    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        return rc;
    }

    if ((this->command & 0xF0) == MQTT_CMD::CONNECT && this->complete_connect) {
        rc = MQTT_RC_CODE::ERR_DUP_CONNECT;
    }

    if ((this->command & 0xF0) != MQTT_CMD::CONNECT &&
        !this->complete_connect) {
        rc = MQTT_RC_CODE::ERR_NOT_CONNECT;
    }

    return rc;
}

template <typename SocketType>
MQTT_RC_CODE MqttSession<SocketType>::check_remaining_length() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    switch (this->command & 0xF0) {
        case MQTT_CMD::PUBACK:
        case MQTT_CMD::PUBREC:
        case MQTT_CMD::PUBREL:
        case MQTT_CMD::PUBCOMP:
        case MQTT_CMD::UNSUBACK: {
            if (this->remaining_length != 2) {
                rc = MQTT_RC_CODE::ERR_MALFORMED_PACKET;
            }
            break;
        }

        case MQTT_CMD::PINGREQ:
        case MQTT_CMD::PINGRESP:
        case MQTT_CMD::DISCONNECT: {
            if (this->remaining_length != 0) {
                rc = MQTT_RC_CODE::ERR_MALFORMED_PACKET;
            }
            break;
        }
        default: {
            break;
        }
    }

    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        SPDLOG_ERROR("Wrong remaning length, value = [{}]",
                     this->remaining_length);
    }

    return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_fixed_header() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    rc = co_await read_byte(&this->command, false);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = check_command();
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = co_await read_remaining_length();
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = check_remaining_length();
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    co_return rc;
}

template <typename SocketType>
MQTT_RC_CODE MqttSession<SocketType>::add_mqtt_fixed_header(std::string& packet,
                                                            uint8_t cmd,
                                                            uint32_t rlen) {
    uint8_t remaining_bytes[5], byte;
    uint8_t remaining_count = 0;

    do {
        byte = rlen % 0x80;
        rlen /= 0x80;

        if (rlen > 0) {
            byte |= 0x80;
        }

        remaining_bytes[remaining_count] = byte;

        remaining_count++;

    } while (rlen > 0 && remaining_count < 5);

    if (remaining_count == 5) {
        return MQTT_RC_CODE::ERR_PAYLOAD_SIZE;
    }

    packet.reserve(1 + remaining_count);
    packet.push_back(cmd);

    // remaining_count 至少为 1
    for (uint8_t idx = 0; idx < remaining_count; idx++) {
        packet.push_back(remaining_bytes[idx]);
    }

    return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_connack(
    uint8_t ack, uint8_t reason_code) {
    asio::error_code ec;
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    SPDLOG_DEBUG("CONNACK: send ack = [X'{:02X}'] reason_code = [X'{:02X}']",
                 static_cast<uint16_t>(ack),
                 static_cast<uint16_t>(reason_code));

    std::string packet;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::CONNACK, 2);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    std::array<asio::const_buffer, 3> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&ack, sizeof(uint8_t)),
         asio::buffer(&reason_code, sizeof(uint8_t))}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_suback(
    uint16_t packet_id, const std::string& payload) {
    asio::error_code ec;
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    std::string packet;
    uint16_t net_packet_id;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::SUBACK, 2 + payload.length());
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    net_packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 3> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&net_packet_id, sizeof(uint16_t)),
         asio::buffer(payload.data(), payload.length())}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("SUBACK: send packet_id = [X'{:04X}']", packet_id);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_unsuback(
    uint16_t packet_id) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string packet;
    uint16_t net_packet_id;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::UNSUBACK, 2);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    net_packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 2> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&net_packet_id, sizeof(uint16_t))}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("UNSUBACK: send packet_id = [X'{:04X}']", packet_id);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_puback(
    uint16_t packet_id) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string packet;
    uint16_t net_packet_id;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::PUBACK, 2);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    net_packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 2> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&net_packet_id, sizeof(uint16_t))}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("PUBACK: send packet_id = [X'{:04X}']", packet_id);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_pubrec(
    uint16_t packet_id) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string packet;
    uint16_t net_packet_id;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::PUBREC, 2);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    net_packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 2> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&net_packet_id, sizeof(uint16_t))}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("PUBREC: send packet_id = [X'{:04X}']", packet_id);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_pubrel(
    uint16_t packet_id) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string packet;
    uint16_t net_packet_id;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::PUBREL | 0x02, 2);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    net_packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 2> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&net_packet_id, sizeof(uint16_t))}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("PUBREL: send packet_id = [X'{:04X}']", packet_id);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_pubcomp(
    uint16_t packet_id) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string packet;
    uint16_t net_packet_id;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::PUBCOMP, 2);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    net_packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 2> buf = {
        {asio::buffer(packet.data(), packet.length()),
         asio::buffer(&net_packet_id, sizeof(uint16_t))}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("PUBCOMP: send packet_id = [X'{:04X}']", packet_id);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_pingresp() {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string packet;

    rc = add_mqtt_fixed_header(packet, MQTT_CMD::PINGRESP, 0);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (this->is_websocket) {
        rc = co_await this->write_websocket(packet);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket,
                             asio::buffer(packet.data(), packet.length()),
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    SPDLOG_DEBUG("PINGRESP");

    co_return rc;
}

template <typename SocketType>
asio::awaitable<void> MqttSession<SocketType>::handle_packet() {
    // 如果是 websocket, 需要进行一次握手
    if (this->is_websocket) {
        this->rc = co_await handle_websocket_handshake();
        if (this->rc != MQTT_RC_CODE::ERR_SUCCESS) {
            handle_error_code();
            co_return;
        }
    }

    for (;;) {
        init_buffer();

        flush_deadline();

        if (this->is_websocket) {
            this->rc = co_await read_websocket();
            if (this->rc != MQTT_RC_CODE::ERR_SUCCESS) {
                break;
            }

            SPDLOG_INFO("command = [X'{:02X}'], remaining_length = [{}]",
                        static_cast<uint16_t>(this->command),
                        this->remaining_length);
        } else {
            this->rc = co_await read_fixed_header();
            if (this->rc != MQTT_RC_CODE::ERR_SUCCESS) {
                break;
            }

            this->rc = co_await read_bytes_to_buf(
                this->payload, this->remaining_length, false);
            if (this->rc != MQTT_RC_CODE::ERR_SUCCESS) {
                break;
            }
        }

        switch (this->command & 0xF0) {
            case MQTT_CMD::CONNECT:
                this->rc = co_await handle_connect();
                break;
            case MQTT_CMD::PUBLISH:
                this->rc = co_await handle_publish();
                break;
            case MQTT_CMD::PUBACK:
                this->rc = co_await handle_puback();
                break;
            case MQTT_CMD::PUBREC:
                this->rc = co_await handle_pubrec();
                break;
            case MQTT_CMD::PUBREL:
                this->rc = co_await handle_pubrel();
                break;
            case MQTT_CMD::PUBCOMP:
                this->rc = co_await handle_pubcomp();
                break;
            case MQTT_CMD::SUBSCRIBE:
                this->rc = co_await handle_subscribe();
                break;
            case MQTT_CMD::UNSUBSCRIBE:
                this->rc = co_await handle_unsubscribe();
                break;
            case MQTT_CMD::PINGREQ:
                this->rc = co_await handle_pingreq();
                break;
            case MQTT_CMD::DISCONNECT:
                this->rc = co_await handle_disconnect();
                break;
            default:
                break;
        }

        if (this->rc == MQTT_RC_CODE::ERR_SUCCESS_DISCONNECT) {
            break;
        }

        if (this->rc != MQTT_RC_CODE::ERR_SUCCESS) {
            break;
        }
    }

    handle_error_code();
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE>
MqttSession<SocketType>::handle_websocket_handshake() {
    asio::error_code ec;
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    const char* method;
    size_t method_len;
    const char* url;
    size_t url_len;
    int minor_version;
    size_t num_headers = 100;
    std::vector<phr_header> headers(num_headers);
    int pret;
    uint8_t sha1buf[20], key_src[60];
    char accept_key[29];

    size_t data_len = co_await asio::async_read_until(
        this->socket, this->head_buf, "\r\n\r\n",
        asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    const char* data = asio::buffer_cast<const char*>(this->head_buf.data());
    pret =
        phr_parse_request(data, data_len, &method, &method_len, &url, &url_len,
                          &minor_version, headers.data(), &num_headers, 0);
    if (pret < 0) {
        SPDLOG_WARN("parse http head failed");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    this->head_buf.consume(data_len);

    auto get_header_value =
        [&headers, num_headers](std::string_view key) -> std::string_view {
        for (size_t i = 0; i < num_headers; i++) {
            if (tolower_equal(key, std::string_view(headers[i].name,
                                                    headers[i].name_len))) {
                return trim_sv(
                    std::string_view(headers[i].value, headers[i].value_len));
            }
        }
        return "";
    };

    if (!tolower_equal(std::string_view(method, method_len), "GET"sv)) {
        SPDLOG_WARN("not GET request");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    if (!tolower_equal(get_header_value("Connection"sv), "Upgrade"sv)) {
        SPDLOG_WARN("not upgrade request");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    if (!tolower_equal(get_header_value("Upgrade"sv), "websocket"sv)) {
        SPDLOG_WARN("not websocket request");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    if (!tolower_equal(get_header_value("Sec-WebSocket-Version"sv), "13"sv)) {
        SPDLOG_WARN("error websocket version");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    if (!tolower_equal(get_header_value("Sec-WebSocket-Protocol"sv),
                       "mqtt"sv)) {
        SPDLOG_WARN("not mqtt request");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    if (atoi(get_header_value("Content-Length"sv).data())) {
        SPDLOG_WARN("content-length must be 0");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    auto ws_key_sv = get_header_value("Sec-WebSocket-Key"sv);
    if (ws_key_sv.length() != 24) {
        SPDLOG_WARN("error Sec-WebSocket-Key value");
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    std::memcpy(key_src, ws_key_sv.data(), 24);
    std::memcpy(key_src + 24, ws_guid, 36);

    sha1_context ctx;
    init(ctx);
    update(ctx, key_src, sizeof(key_src));
    finish(ctx, sha1buf);

    utils::base64_encode(accept_key, sha1buf, sizeof(sha1buf), 0);

    // clang-format off
    auto resp_content =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: " + std::string(accept_key, 28) + "\r\n"
    "Sec-WebSocket-Protocol: mqtt\r\n"
    "Upgrade: websocket\r\n"
    "\r\n";
    // clang-format on

    co_await async_write(
        this->socket, asio::buffer(resp_content.data(), resp_content.length()),
        asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::read_websocket() {
    asio::error_code ec;
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;
    bool is_new_frame = true;
    bool is_read_cmd = false;
    bool is_read_remaining_length = false;
    uint32_t remaining_count = 0;
    uint32_t remaining_mult = 1;

    while (true) {
        if (is_new_frame) {
            co_await async_read(this->socket, this->head_buf,
                                asio::transfer_exactly(SHORT_HEADER),
                                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) {
                co_return MQTT_RC_CODE::ERR_NO_CONN;
            }
            is_new_frame = false;
        }

        const char* data_ptr =
            asio::buffer_cast<const char*>(this->head_buf.data());
        auto status = this->ws.parse_header(data_ptr, this->ws.len_bytes());
        if (status == ws_header_status::complete) {
            this->ws.reset_len_bytes();
            this->head_buf.consume(this->head_buf.size());

            auto ws_payload_length = this->ws.payload_length();

            if (ws_payload_length == 0) {
                is_new_frame = true;
                continue;
            }

            auto old_payload_length = this->payload.length();
            auto new_payload_length = old_payload_length + ws_payload_length;

            this->payload.resize(new_payload_length);

            co_await async_read(
                this->socket,
                asio::buffer(this->payload.data() + old_payload_length,
                             ws_payload_length),
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) {
                co_return MQTT_RC_CODE::ERR_NO_CONN;
            }

            std::span<char> ws_payload(
                this->payload.data() + old_payload_length, ws_payload_length);

            ws_frame_type type = ws.parse_payload(ws_payload);
            switch (type) {
                case ws_frame_type::WS_INCOMPLETE_BINARY_FRAME:
                case ws_frame_type::WS_BINARY_FRAME: {
                    break;
                }
                case ws_frame_type::WS_CLOSE_FRAME: {
                    close_frame cf = ws.parse_close_payload(ws_payload.data(),
                                                            ws_payload.size());
                    std::string close_msg = ws.format_close_payload(
                        close_code::normal, cf.message, cf.length);
                    co_await this->write_websocket(close_msg, opcode::close);
                    this->disconnect();
                    co_return MQTT_RC_CODE::ERR_SUCCESS_DISCONNECT;
                }
                default: {
                    SPDLOG_WARN("Unsupported WebSocket frames: {}",
                                static_cast<uint32_t>(type));
                    co_return MQTT_RC_CODE::ERR_PROTOCOL;
                }
            }

            if (!is_read_cmd && this->pos < this->payload.length()) {
                this->command = this->payload[this->pos];
                this->pos++;

                rc = this->check_command();
                if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                    co_return rc;
                }

                is_read_cmd = true;
                new_payload_length--;
            }

            if (!is_read_remaining_length) {
                for (;
                     this->pos < this->payload.length() && remaining_count < 4;
                     remaining_count++, remaining_mult <<= 7) {
                    auto byte = this->payload[this->pos];
                    this->pos++;

                    remaining_length += (byte & 0x7F) * remaining_mult;

                    if (!(byte & 0x80)) {
                        is_read_remaining_length = true;
                        this->remaining_length = remaining_length;
                        std::memmove(this->payload.data(),
                                     this->payload.data() + this->pos,
                                     this->payload.length() - this->pos);
                        this->payload.resize(this->payload.length() -
                                             this->pos);
                        this->pos = 0;
                        break;
                    }
                }
            }

            if (is_read_remaining_length) {
                if (this->payload.length() > this->remaining_length) {
                    co_return MQTT_RC_CODE::ERR_REMAINING_LENGTH;
                }

                if (this->payload.length() == this->remaining_length) {
                    break;
                }
            }

            is_new_frame = true;
        } else if (status == ws_header_status::incomplete) {
            co_await async_read(
                this->socket, this->head_buf,
                asio::transfer_exactly(this->ws.left_header_len()),
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) {
                co_return MQTT_RC_CODE::ERR_NO_CONN;
            }
        } else {
            SPDLOG_WARN("ws_header_status error");
            co_return MQTT_RC_CODE::ERR_PROTOCOL;
        }
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::write_websocket(
    std::string_view msg, opcode op, bool eof) {
    asio::error_code ec;
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    std::vector<asio::const_buffer> buffers;
    std::string_view header;

    header = ws.encode_ws_header(msg.length(), op, eof, false, false);
    buffers.push_back(asio::buffer(header));
    buffers.push_back(asio::buffer(msg));

    co_await async_write(this->socket, buffers,
                         asio::redirect_error(asio::use_awaitable, ec));
    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    co_return rc;
}

template <typename SocketType>
template <std::size_t N>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::write_websocket(
    const std::array<asio::const_buffer, N>& msg, opcode op, bool eof) {
    asio::error_code ec;
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    std::vector<asio::const_buffer> buffers;
    std::string_view header;

    std::size_t total_size = 0;
    for (const auto& buf : msg) {
        total_size += asio::buffer_size(buf);
    }
    header = ws.encode_ws_header(total_size, op, eof, false, false);
    buffers.push_back(asio::buffer(header));

    buffers.insert(buffers.end(), msg.begin(), msg.end());

    co_await async_write(this->socket, buffers,
                         asio::redirect_error(asio::use_awaitable, ec));

    if (ec) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_connect() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;
    uint16_t protocol_name_length;
    std::string protocol_name;
    uint8_t protocol_version;
    uint8_t connect_flags;
    bool clean_session, will;
    mqtt_packet_t will_topic;
    uint8_t password_flag, username_flag;
    std::string password, username;
    uint16_t keep_alive;
    std::string client_id;
    bool session_present;

    SPDLOG_DEBUG("CONNECT: read protocol name length");
    rc = co_await read_uint16(&protocol_name_length, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (protocol_name_length != 4) {
        SPDLOG_ERROR("protocol_name_length = [{}]", protocol_name_length);
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    SPDLOG_DEBUG("CONNECT: read protocol name");
    rc = co_await read_bytes_to_buf(protocol_name, protocol_name_length, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (protocol_name != "MQTT") {
        SPDLOG_ERROR("protocol_name = [{}]", protocol_name);
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    SPDLOG_DEBUG("CONNECT: read protocol version");
    rc = co_await read_byte(&protocol_version, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (protocol_version != 0x04) {
        rc =
            co_await send_connack(0x00, MQTT_CONNACK::REFUSED_PROTOCOL_VERSION);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        SPDLOG_ERROR("protocol_version = [{}]",
                     static_cast<uint16_t>(protocol_version));

        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    SPDLOG_DEBUG("CONNECT: read connect flags");
    rc = co_await read_byte(&connect_flags, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    clean_session = (connect_flags & 0x02) >> 1;
    will = connect_flags & 0x04;
    will_topic.qos = (connect_flags & 0x18) >> 3;
    will_topic.retain = ((connect_flags & 0x20) == 0x20);
    password_flag = connect_flags & 0x40;
    username_flag = connect_flags & 0x80;

    if (will_topic.qos == 3) {
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    if (will == false && (will_topic.qos != 0 || will_topic.retain)) {
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    SPDLOG_DEBUG("CONNECT: read keep alive");
    rc = co_await read_uint16(&keep_alive, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    // KeepAlive 时间
    SPDLOG_DEBUG("keep alive = [{}] seconds", keep_alive);

    SPDLOG_DEBUG("CONNECT: read client identifier");
    rc = co_await read_utf8_string(client_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    // MS_ 前缀用于自动生成的 client_id, 客户端生成的不能带有 MS_ 前缀
    if (client_id.starts_with("MS_")) {
        rc = co_await send_connack(0x00,
                                   MQTT_CONNACK::REFUSED_IDENTIFIER_REJECTED);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
        co_return MQTT_RC_CODE::ERR_BAD_CLIENT_ID;
    }

    if (client_id.empty()) {
        // 不能保留会话
        if (clean_session == false) {
            rc = co_await send_connack(
                0x00, MQTT_CONNACK::REFUSED_IDENTIFIER_REJECTED);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                co_return rc;
            }
            co_return MQTT_RC_CODE::ERR_PROTOCOL;
        }

        // 服务端生成一个 cliend_id
        client_id = this->broker.gen_session_id();
    }

    // 读取遗嘱消息
    if (will) {
        rc = co_await read_will_packet(will_topic, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    }

    // 读取用户名
    if (username_flag) {
        rc = co_await read_utf8_string(username, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    }

    // 读取密码
    if (password_flag) {
        rc = co_await read_utf8_string(password, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    }

    // 进行密码校验
    if (MqttConfig::getInstance()->auth()) {
        if (MqttConfig::getInstance()->auth(username, password) == false) {
            rc = co_await send_connack(
                0x00, MQTT_CONNACK::REFUSED_BAD_USERNAME_PASSWORD);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                co_return rc;
            }
            co_return MQTT_RC_CODE::ERR_BAD_USERNAME_PASSWORD;
        }

        this->username = username;
    }

    // 进行 ACL 校验
    if (MqttConfig::getInstance()->acl_enable()) {
        mqtt_acl_rule_t rule;
        rule.type = MQTT_ACL_TYPE::IPADDR;
        asio::error_code ec;

#ifdef MQ_WITH_TLS
        if constexpr (std::is_same_v<SocketType, asio::ssl::stream<
                                                     asio::ip::tcp::socket>>) {
            rule.object = this->socket.lowest_layer()
                              .remote_endpoint(ec)
                              .address()
                              .to_string();
        } else {
            rule.object =
                this->socket.remote_endpoint(ec).address().to_string();
        }
#else
        rule.object = this->socket.remote_endpoint(ec).address().to_string();
#endif
        if (ec) {
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }

        SPDLOG_DEBUG("remote ip addr : [{}]", rule.object);

        if (!MqttConfig::getInstance()->acl_check(rule)) {
            rc = co_await send_connack(0x00,
                                       MQTT_CONNACK::REFUSED_NOT_AUTHORIZED);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                co_return rc;
            }
            co_return MQTT_RC_CODE::ERR_REFUSED_NOT_AUTHORIZED;
        }

        rule.type = MQTT_ACL_TYPE::CLIENTID;
        rule.object = client_id;

        if (!MqttConfig::getInstance()->acl_check(rule)) {
            rc = co_await send_connack(0x00,
                                       MQTT_CONNACK::REFUSED_NOT_AUTHORIZED);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                co_return rc;
            }
            co_return MQTT_RC_CODE::ERR_REFUSED_NOT_AUTHORIZED;
        }
    }

    this->client_id = client_id;

    this->session_state.clean_session = clean_session;
    this->session_state.keep_alive = keep_alive;
    this->session_state.will_topic = will_topic;

    // 加入 broker
    session_present = this->broker.join_or_update(this->shared_from_this());

    // 发送 CONNACK 响应
    rc = co_await send_connack(session_present, MQTT_CONNACK::ACCEPTED);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return MQTT_RC_CODE::ERR_NO_CONN;
    }

    // CONNECT 完成标志设置
    this->complete_connect = true;

    // 添加自动订阅项
    add_subscribe(MqttConfig::getInstance()->auto_subscribe_list());

    // 开启协程用于处理需要当前会话转发的主题
    asio::co_spawn(
        this->socket.get_executor(),
        [self = this->shared_from_this()] {
            return self->handle_inflighting_packets();
        },
        asio::detached);

    // 开启协程用于处理需要等待一段时间的操作
    asio::co_spawn(
        this->socket.get_executor(),
        [self = this->shared_from_this()] {
            return self->handle_waiting_map_packets();
        },
        asio::detached);

    SPDLOG_DEBUG(
        "success to handle `CONNECT`, client_id = [{}], clean_session = [{}]",
        this->client_id, this->session_state.clean_session);

    co_return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_publish() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;
    mqtt_packet_t pub_packet;
    uint8_t retain = this->command & 0x01;    // 第 0 位 retain
    uint8_t qos =
        (this->command >> 1) & 0x03;    // 第 2 位 Qos-H, 第 1 位 Qos-S
    uint8_t dup = (this->command >> 3) & 1;    // 第 3 位 dup
    std::string pub_topic;
    uint16_t packet_id;
    uint32_t pub_payloadlen;
    std::string pub_payload;

    if (qos == 3 || (qos == 1 && dup == 1)) {
        co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
    }

    rc = co_await read_utf8_string(pub_topic, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    rc = check_pub_topic(pub_topic);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (pub_topic.empty()) {
        co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
    }

    if (qos > 0) {
        rc = co_await read_uint16(&packet_id, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        if (packet_id == 0) {
            co_return MQTT_RC_CODE::ERR_PROTOCOL;
        }
    }

    // 允许主题内容为空, 对于保留消息来说, 内容为空就删除保留消息
    pub_payloadlen = this->remaining_length - this->pos;
    if (pub_payloadlen > 0) {
        rc = co_await read_bytes_to_buf(pub_payload, pub_payloadlen, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    }

    // 报文读取完毕, 对于 qos1 和 qos2 级别需要发送响应报文
    if (qos == 1) {
        rc = co_await send_puback(packet_id);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else if (qos == 2) {
        rc = co_await send_pubrec(packet_id);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        // 对于 qos2 级别, 如果客户端没有收到 PUBREC 报文, 可能会重传
        // 因此只要 packet_id 还没被服务端释放, 就不再接受重传的报文
        // 保证只有一个消息到达
        if (dup == 1 && this->session_state.waiting_map.contains(packet_id)) {
            co_return rc;
        }

        // 等待 PUBREL
        mqtt_packet_t packet;
        packet.state = MQTT_MSG_STATE::WAIT_RECEIVE_PUBREL;
        packet.expiry_time =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(MqttConfig::getInstance()->max_waiting_time());

        this->session_state.waiting_map[packet_id] = std::move(packet);
    }

    // ACL 检查
    if (MqttConfig::getInstance()->acl_enable()) {
        mqtt_acl_rule_t rule;
        rule.type = MQTT_ACL_TYPE::USERNAME;
        rule.object = this->username;
        rule.action = MQTT_ACL_ACTION::PUB;
        rule.topics = std::make_unique<std::unordered_set<std::string>>();

        rule.topics->emplace(pub_topic);

        // 检查不通过则不再对此消息处理
        if (!MqttConfig::getInstance()->acl_check(rule)) {
            co_return MQTT_RC_CODE::ERR_SUCCESS;
        }
    }

    // 组包
    pub_packet.dup = dup;
    pub_packet.qos = qos;
    pub_packet.retain = retain;
    pub_packet.topic_name =
        std::make_shared<const std::string>(std::move(pub_topic));
    pub_packet.payload =
        std::make_shared<const std::string>(std::move(pub_payload));

    // 添加到保留消息
    if (retain) {
        // 内容为空则移除保留消息
        if (pub_payloadlen == 0) {
            this->broker.remove_retain(*(pub_packet.topic_name));
        } else {
            this->broker.add_retain(pub_packet);
        }
    }

    // 注意要把 dup 和 retain 标志重置为 0, 因为是作为新消息发送的
    pub_packet.dup = 0;
    pub_packet.retain = 0;

    // 消息分发
    this->broker.dispatch(pub_packet);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_puback() {
    MQTT_RC_CODE rc;
    uint16_t packet_id;

    rc = co_await read_uint16(&packet_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (packet_id == 0) {
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    // 查找对应的主题
    auto iter = this->session_state.waiting_map.find(packet_id);
    if (iter == this->session_state.waiting_map.end()) {
        // 没找到说明可能已经被服务端删除了, 正常返回即可
        co_return rc;
    }

    SPDLOG_DEBUG(
        "PUBACK: receive puback, packet id = [X'{:04X}'], state = [{}]",
        packet_id, static_cast<uint16_t>(iter->second.state));

    // 完成 Qos1 交互, 释放 packet id
    this->session_state.waiting_map.erase(iter);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_pubrec() {
    MQTT_RC_CODE rc;
    uint16_t packet_id;

    rc = co_await read_uint16(&packet_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (packet_id == 0) {
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    // 查找对应的主题
    auto iter = this->session_state.waiting_map.find(packet_id);
    if (iter == this->session_state.waiting_map.end()) {
        // 没找到说明可能已经被服务端删除了, 正常返回即可
        co_return rc;
    }

    SPDLOG_DEBUG(
        "PUBREC: receive pubrec, packet id = [X'{:04X}'], state = [{}]",
        packet_id, static_cast<uint16_t>(iter->second.state));

    mqtt_packet_t& packet = iter->second;

    // 收到了 PUBREC 后不能再重发 PUBLISH 了
    // 更改状态, 避免切换协程后重发 PUBLISH
    packet.state = MQTT_MSG_STATE::WAIT_RECEIVE_PUBREC;
    packet.expiry_time = std::chrono::steady_clock::time_point::max();

    // 发送 PUBREL 响应
    rc = co_await send_pubrel(packet_id);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    // 设置重发
    packet.state = MQTT_MSG_STATE::WAIT_RESEND_PUBREL;
    packet.max_resend_count = MqttConfig::getInstance()->max_resend_count();
    packet.expiry_time =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(MqttConfig::getInstance()->resend_duration());

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_pubrel() {
    MQTT_RC_CODE rc;
    uint16_t packet_id;

    rc = co_await read_uint16(&packet_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (packet_id == 0) {
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    // 查找对应的主题
    auto iter = this->session_state.waiting_map.find(packet_id);
    if (iter == this->session_state.waiting_map.end()) {
        // 没找到说明可能已经被服务端删除了, 正常返回即可
        co_return rc;
    }

    SPDLOG_DEBUG(
        "PUBREL: receive pubrel, packet id = [X'{:04X}'], state = [{}]",
        packet_id, static_cast<uint16_t>(iter->second.state));

    mqtt_packet_t& packet = iter->second;

    // 发送 PUBCOMP 响应
    rc = co_await send_pubcomp(packet_id);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        // 发送失败设置重发
        packet.state = MQTT_MSG_STATE::WAIT_RESEND_PUBCOMP;
        packet.max_resend_count = MqttConfig::getInstance()->max_resend_count();
        packet.expiry_time =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(MqttConfig::getInstance()->resend_duration());
        co_return rc;
    }

    // 发送成功则释放 packet id
    this->session_state.waiting_map.erase(iter);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_pubcomp() {
    MQTT_RC_CODE rc;
    uint16_t packet_id;

    rc = co_await read_uint16(&packet_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (packet_id == 0) {
        co_return MQTT_RC_CODE::ERR_PROTOCOL;
    }

    // 查找对应的主题
    auto iter = this->session_state.waiting_map.find(packet_id);
    if (iter == this->session_state.waiting_map.end()) {
        // 没找到说明可能已经被服务端删除了, 正常返回即可
        co_return rc;
    }

    SPDLOG_DEBUG(
        "PUBCOMP: receive pubcomp, packet id = [X'{:04X}'], state = [{}]",
        packet_id, static_cast<uint16_t>(iter->second.state));

    // 完成 Qos2 交互, 释放 packet id
    this->session_state.waiting_map.erase(iter);

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_subscribe() {
    MQTT_RC_CODE rc;
    uint16_t packet_id;
    std::string tmp_topic;
    uint8_t tmp_qos;
    std::list<std::pair<std::string, uint8_t>> sub_topic_list;
    std::string suback_payload;

    rc = co_await read_uint16(&packet_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    if (packet_id == 0) {
        co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
    }

    // 必须包含至少一对主题过滤器 和 QoS等级字段组合
    if (this->pos == this->remaining_length) {
        co_return MQTT_RC_CODE::ERR_MALFORMED_PACKET;
    }

    while (this->pos < this->remaining_length) {
        rc = co_await read_utf8_string(tmp_topic, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        rc = check_sub_topic(tmp_topic);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        rc = co_await read_byte(&tmp_qos, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        if (tmp_qos > 2) {
            co_return MQTT_RC_CODE::ERR_PROTOCOL;
        }

        if (MqttConfig::getInstance()->acl_enable()) {
            mqtt_acl_rule_t rule;
            rule.type = MQTT_ACL_TYPE::USERNAME;
            rule.object = this->username;
            rule.action = MQTT_ACL_ACTION::SUB;
            rule.topics = std::make_unique<std::unordered_set<std::string>>();

            rule.topics->emplace(tmp_topic);

            if (MqttConfig::getInstance()->acl_check(rule)) {
                sub_topic_list.emplace_back(std::move(tmp_topic), tmp_qos);
            } else {
                tmp_qos = 0x80;    // 表示失败
            }

        } else {
            sub_topic_list.emplace_back(std::move(tmp_topic), tmp_qos);
        }

        suback_payload.push_back(tmp_qos);
    }

    rc = co_await send_suback(packet_id, suback_payload);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    // 添加订阅项
    add_subscribe(sub_topic_list);

    co_return MQTT_RC_CODE::ERR_SUCCESS;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_unsubscribe() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS_DISCONNECT;

    uint16_t packet_id;
    std::string tmp_topic;
    std::list<std::string> unsub_topic_list;

    rc = co_await read_uint16(&packet_id, true);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    while (this->pos < this->remaining_length) {
        rc = co_await read_utf8_string(tmp_topic, true);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }

        unsub_topic_list.emplace_back(std::move(tmp_topic));
    }

    // 读取完后发送响应
    rc = co_await send_unsuback(packet_id);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    // 删除对应主题订阅信息
    for (auto& name : unsub_topic_list) {
        this->session_state.sub_topic_map.erase(name);
    }

    if (this->session_state.sub_topic_map.empty()) {
        MqttSession<SocketType>::active_sub_set.erase(this->client_id);
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_pingreq() {
    MQTT_RC_CODE rc;

    rc = co_await send_pingresp();
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::handle_disconnect() {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS_DISCONNECT;

    disconnect();

    // 删除会话关联的遗嘱消息
    this->session_state.will_topic = mqtt_packet_t{};

    co_return rc;
}

template <typename SocketType>
asio::awaitable<void> MqttSession<SocketType>::handle_inflighting_packets() {
    MQTT_RC_CODE rc;
    mqtt_packet_t packet;
    uint8_t old_qos;
    std::list<mqtt_packet_t> send_packet_list;
    asio::error_code ec;

    while (this->is_open()) {
        if (!this->session_state.inflight_queue.empty()) {
            packet = this->session_state.inflight_queue.front();
            this->session_state.inflight_queue.pop();

            for (auto& [sub_topic, qos] : this->session_state.sub_topic_map) {
                // 保留消息指定了具体的主题名称
                if (packet.retain && packet.specified_topic_name.length() &&
                    sub_topic != packet.specified_topic_name) {
                    continue;
                }

                if (utils::check_topic_match(*(packet.topic_name), sub_topic)) {
                    // Qos 等级取两者最小值
                    old_qos = packet.qos;

                    packet.qos = std::min(packet.qos, qos);

                    send_packet_list.emplace_back(packet);

                    packet.qos = old_qos;
                }
            }

            // 批量发送满足订阅的消息
            rc = co_await send_mqtt_packets(send_packet_list);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                disconnect();
            }

            send_packet_list.clear();

        } else {
            co_await this->cond_timer.async_wait(
                asio::redirect_error(asio::use_awaitable, ec));
        }
    }
}

template <typename SocketType>
asio::awaitable<void> MqttSession<SocketType>::handle_waiting_map_packets() {
    MQTT_RC_CODE rc;
    auto check_duration = std::chrono::seconds(
        MqttConfig::getInstance()->check_waiting_map_duration());

    // 每到一次间隔时间检查一下 waiting
    while (this->is_open()) {
        this->check_timer.expires_after(check_duration);

        co_await this->check_timer.async_wait(asio::use_awaitable);

        // 在这个阶段可能会连续发送多个报文
        // 如果协程切换到 handle_inflighting_packets 执行发送
        // 可能会导致我们在遍历 map 的途中切换过去新插入了元素
        // 因此使用 channel 进行写同步, 当 map 中的发送都处理完了才能切换
        if (!this->write_lock.try_send()) {
            co_await this->write_lock.async_send(asio::deferred);
        }

        auto deadline = std::chrono::steady_clock::now();

        for (auto iter = this->session_state.waiting_map.begin();
             iter != this->session_state.waiting_map.end();) {
            // 使用引用, 方便直接修改表中的内容
            mqtt_packet_t& packet = iter->second;

            if (packet.expiry_time > deadline) {
                iter++;
                continue;
            }

            if (packet.state == MQTT_MSG_STATE::WAIT_RECEIVE_PUBACK ||
                packet.state == MQTT_MSG_STATE::WAIT_RECEIVE_PUBCOMP ||
                packet.state == MQTT_MSG_STATE::WAIT_RECEIVE_PUBREC ||
                packet.state == MQTT_MSG_STATE::WAIT_RECEIVE_PUBREL) {
                // 删除超过最长等待时间的报文
                iter = this->session_state.waiting_map.erase(iter);
            } else if (packet.state ==
                       MQTT_MSG_STATE::WAIT_RESEND_PUBLISH_QOS1) {
                if (packet.max_resend_count == 0) {
                    iter = this->session_state.waiting_map.erase(iter);
                } else {
                    rc = co_await send_publish_qos1(packet, false);
                    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                        break;
                    }

                    packet.max_resend_count--;

                    packet.expiry_time =
                        std::chrono::steady_clock::now() +
                        std::chrono::seconds(
                            MqttConfig::getInstance()->resend_duration());
                    iter++;
                }

            } else if (packet.state ==
                       MQTT_MSG_STATE::WAIT_RESEND_PUBLISH_QOS2) {
                if (packet.max_resend_count == 0) {
                    iter = this->session_state.waiting_map.erase(iter);
                } else {
                    rc = co_await send_publish_qos2(packet, false);
                    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                        break;
                    }

                    packet.max_resend_count--;

                    packet.expiry_time =
                        std::chrono::steady_clock::now() +
                        std::chrono::seconds(
                            MqttConfig::getInstance()->resend_duration());
                    iter++;
                }
            } else if (packet.state == MQTT_MSG_STATE::WAIT_RESEND_PUBREL) {
                if (packet.max_resend_count == 0) {
                    iter = this->session_state.waiting_map.erase(iter);
                } else {
                    rc = co_await send_pubrel(packet.packet_id);
                    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                        break;
                    }

                    packet.max_resend_count--;

                    packet.expiry_time =
                        std::chrono::steady_clock::now() +
                        std::chrono::seconds(
                            MqttConfig::getInstance()->resend_duration());
                    iter++;
                }
            } else if (packet.state == MQTT_MSG_STATE::WAIT_RESEND_PUBCOMP) {
                if (packet.max_resend_count == 0) {
                    iter = this->session_state.waiting_map.erase(iter);
                } else {
                    rc = co_await send_pubcomp(packet.packet_id);
                    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                        break;
                    }

                    packet.max_resend_count--;

                    packet.expiry_time =
                        std::chrono::steady_clock::now() +
                        std::chrono::seconds(
                            MqttConfig::getInstance()->resend_duration());
                    iter++;
                }
            } else {
                iter++;
            }
        }

        // 释放写锁
        this->write_lock.try_receive([](auto...) {});
    }
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_mqtt_packets(
    const std::list<mqtt_packet_t>& packet_list) {
    MQTT_RC_CODE rc = MQTT_RC_CODE::ERR_SUCCESS;

    if (packet_list.empty()) {
        co_return rc;
    }

    // 写同步, 在将列表中的主题写完后才能调度到 handle_waiting_map_packets
    if (!this->write_lock.try_send()) {
        co_await this->write_lock.async_send(asio::deferred);
    }

    for (auto& packet : packet_list) {
        if (packet.qos == 0) {
            co_await send_publish_qos0(packet);
        } else if (packet.qos == 1) {
            rc = co_await send_publish_qos1(packet, true);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                break;
            }
        } else {
            rc = co_await send_publish_qos2(packet, true);
            if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
                break;
            }
        }
    }

    this->write_lock.try_receive([](auto...) {});

    co_return rc;
}

template <typename SocketType>
asio::awaitable<void> MqttSession<SocketType>::send_publish_qos0(
    mqtt_packet_t packet) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string header;
    uint8_t dup = packet.dup;
    uint8_t qos = packet.qos;
    uint8_t retain = packet.retain;
    uint8_t command = static_cast<uint8_t>(
        MQTT_CMD::PUBLISH | static_cast<uint8_t>((dup & 1) << 3) |
        static_cast<uint8_t>(qos << 1) | retain);
    const std::string& topic_name = *(packet.topic_name);
    const std::string& payload = *(packet.payload);
    uint16_t sub_topic_length = topic_name.length();
    uint32_t pub_remaning_length =
        sizeof(sub_topic_length) + sub_topic_length + payload.length();

    SPDLOG_DEBUG("PUBLISH Qos0: command = [X'{:02X}']",
                 static_cast<uint16_t>(command));

    rc = add_mqtt_fixed_header(header, command, pub_remaning_length);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return;
    }

    sub_topic_length =
        asio::detail::socket_ops::host_to_network_short(sub_topic_length);

    std::array<asio::const_buffer, 4> buf = {
        {asio::buffer(header.data(), header.length()),
         asio::buffer(&sub_topic_length, sizeof(uint16_t)),
         asio::buffer(topic_name.data(), topic_name.length()),
         asio::buffer(payload.data(), payload.length())}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            SPDLOG_WARN("Failed to publish topic = [{}]",
                        std::string(topic_name));
            co_return;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            SPDLOG_WARN("Failed to publish topic = [{}]",
                        std::string(topic_name));
            co_return;
        }
    }

    co_return;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_publish_qos1(
    mqtt_packet_t packet, bool is_new) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string header;
    uint8_t dup = packet.dup;
    uint8_t qos = packet.qos;
    uint8_t retain = packet.retain;
    uint8_t command = static_cast<uint8_t>(
        MQTT_CMD::PUBLISH | static_cast<uint8_t>((dup & 1) << 3) |
        static_cast<uint8_t>(qos << 1) | retain);
    const std::string& topic_name = *(packet.topic_name);
    const std::string& payload = *(packet.payload);
    uint16_t sub_topic_length = topic_name.length();
    uint32_t pub_remaning_length =
        sizeof(sub_topic_length) + sub_topic_length + 2 + payload.length();
    uint16_t packet_id;

    // 如果是第一次发送, 取一个未使用的报文标识符
    // 否则用之前生成的报文标识符重发
    if (is_new) {
        packet_id = gen_packet_id();
        packet.packet_id = packet_id;

        // 先将状态存放, 如果 PUBLISH 发送失败需要重发
        packet.dup = 1;
        packet.state =
            MQTT_MSG_STATE::WAIT_RESEND_PUBLISH_QOS1;    // 状态为等待重发
        packet.max_resend_count = MqttConfig::getInstance()->max_resend_count();
        packet.expiry_time =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(MqttConfig::getInstance()->resend_duration());
        this->session_state.waiting_map[packet_id] = packet;
    } else {
        packet_id = packet.packet_id;
    }

    SPDLOG_DEBUG("PUBLISH Qos1: command = [X'{:02X}'], packet id = [X'{:04X}']",
                 static_cast<uint16_t>(command), packet_id);

    rc = add_mqtt_fixed_header(header, command, pub_remaning_length);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    sub_topic_length =
        asio::detail::socket_ops::host_to_network_short(sub_topic_length);
    packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 5> buf = {
        {asio::buffer(header.data(), header.length()),
         asio::buffer(&sub_topic_length, sizeof(uint16_t)),
         asio::buffer(topic_name.data(), topic_name.length()),
         asio::buffer(&packet_id, sizeof(uint16_t)),
         asio::buffer(payload.data(), payload.length())}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            SPDLOG_WARN("Failed to publish topic = [{}]",
                        std::string(topic_name));
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    co_return rc;
}

template <typename SocketType>
asio::awaitable<MQTT_RC_CODE> MqttSession<SocketType>::send_publish_qos2(
    mqtt_packet_t packet, bool is_new) {
    asio::error_code ec;
    MQTT_RC_CODE rc;
    std::string header;
    uint8_t dup = packet.dup;
    uint8_t qos = packet.qos;
    uint8_t retain = packet.retain;
    uint8_t command = static_cast<uint8_t>(
        MQTT_CMD::PUBLISH | static_cast<uint8_t>((dup & 1) << 3) |
        static_cast<uint8_t>(qos << 1) | retain);
    const std::string& topic_name = *(packet.topic_name);
    const std::string& payload = *(packet.payload);
    uint16_t sub_topic_length = topic_name.length();
    uint32_t pub_remaning_length =
        sizeof(sub_topic_length) + sub_topic_length + 2 + payload.length();
    uint16_t packet_id;

    // 如果是第一次发送, 取一个未使用的报文标识符
    // 否则用之前生成的报文标识符重发
    if (is_new) {
        packet_id = gen_packet_id();
        packet.packet_id = packet_id;

        // 先将状态存放, 如果 PUBLISH 发送失败需要重发
        packet.dup = 1;
        packet.state =
            MQTT_MSG_STATE::WAIT_RESEND_PUBLISH_QOS2;    // 状态为等待重发
        packet.max_resend_count = MqttConfig::getInstance()->max_resend_count();
        packet.expiry_time =
            std::chrono::steady_clock::now() +
            std::chrono::seconds(MqttConfig::getInstance()->resend_duration());
        this->session_state.waiting_map[packet_id] = packet;
    } else {
        packet_id = packet.packet_id;
    }

    SPDLOG_DEBUG("PUBLISH Qos2: command = [X'{:02X}'], packet id = [X'{:04X}']",
                 static_cast<uint16_t>(command), packet_id);

    rc = add_mqtt_fixed_header(header, command, pub_remaning_length);
    if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
        co_return rc;
    }

    sub_topic_length =
        asio::detail::socket_ops::host_to_network_short(sub_topic_length);
    packet_id = asio::detail::socket_ops::host_to_network_short(packet_id);

    std::array<asio::const_buffer, 5> buf = {
        {asio::buffer(header.data(), header.length()),
         asio::buffer(&sub_topic_length, sizeof(uint16_t)),
         asio::buffer(topic_name.data(), topic_name.length()),
         asio::buffer(&packet_id, sizeof(uint16_t)),
         asio::buffer(payload.data(), payload.length())}};

    if (this->is_websocket) {
        rc = co_await this->write_websocket(buf);
        if (rc != MQTT_RC_CODE::ERR_SUCCESS) {
            co_return rc;
        }
    } else {
        co_await async_write(this->socket, buf,
                             asio::redirect_error(asio::use_awaitable, ec));
        if (ec) {
            SPDLOG_WARN("Failed to publish topic = [{}]",
                        std::string(topic_name));
            co_return MQTT_RC_CODE::ERR_NO_CONN;
        }
    }

    co_return rc;
}

template <typename SocketType>
void MqttSession<SocketType>::add_subscribe(
    const std::list<std::pair<std::string, uint8_t>>& sub_topic_list) {
    if (sub_topic_list.empty()) {
        return;
    }

    // 更新存在订阅的会话
    MqttSession::active_sub_set.emplace(this->client_id);

    for (const auto& [name, qos] : sub_topic_list) {
        SPDLOG_DEBUG("subscribe topic name = [{}], qos = [X'{:02X}']", name,
                     static_cast<uint16_t>(qos));

        this->session_state.sub_topic_map[name] = qos;

        // 获取保留消息, 只针对当前新增的主题
        this->broker.get_retain(this->shared_from_this(), name);
    }
}

template <typename SocketType>
bool MqttSession<SocketType>::is_open() {
#ifdef MQ_WITH_TLS
    if constexpr (std::is_same_v<SocketType,
                                 asio::ssl::stream<asio::ip::tcp::socket>>) {
        return this->socket.next_layer().is_open();
    } else {
        return this->socket.is_open();
    }
#else
    return this->socket.is_open();
#endif
}