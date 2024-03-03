#include <iostream>
#include <vector>
#include <utility>
#include <chrono>

#include "gtest/gtest.h"
#include "asio.hpp"

using asio::ip::tcp;


// 测试发送错误的协议版本
TEST(MQTT_CONNECT_TEST, protocol_version) {
    uint8_t command = 0x10; // CONNECT
    // 2 + 4 + 1
    std::vector<uint8_t> remaning_length_bytes = { 0x07 };
    // 需要用网络字节序, 小端 0x0004 转大端 0x0400
    uint16_t protocol_name_length = 0x0400;
    std::string protocol_name = "MQTT";
    uint8_t protocol_version = 0x02; // error protocol version

    std::array<asio::const_buffer, 5> buf = {
        {asio::buffer(&command, 1), asio::buffer(remaning_length_bytes.data(), 1),
         asio::buffer(&protocol_name_length, 2), asio::buffer(protocol_name.data(), 4),
         asio::buffer(&protocol_version, 1)
        }};

    asio::io_context io_context;

    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    asio::connect(s, resolver.resolve("localhost", "1883"));

    asio::error_code ec;
    asio::write(s, buf, ec);
    if (ec) {
        ASSERT_TRUE(false) << ec.message();
    }

    uint8_t read_buf[4] = {0};

    auto nread = asio::read(s, asio::buffer(read_buf, 4), ec);
    if (ec) {
        ASSERT_TRUE(false) << ec.message();
    }

    ASSERT_EQ(nread, 4) << "CONNACK 报文长度不正确: " << nread;
    ASSERT_EQ(read_buf[0], 0x20) << "CONNACK 报文类型: "<< static_cast<uint16_t>(read_buf[0]);
    ASSERT_EQ(read_buf[1], 2) << "剩余长度字段不正确: " << static_cast<uint16_t>(read_buf[1]);
    ASSERT_EQ(read_buf[2], 0x00) << "连接确认标志: " << static_cast<uint16_t>(read_buf[2]);
    ASSERT_EQ(read_buf[3], 0x01) << "连接返回码: " << static_cast<uint16_t>(read_buf[3]);

    s.close(ec);
}



int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}