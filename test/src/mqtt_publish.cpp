#include <iostream>

#include "gtest/gtest.h"
#include "mosquitto.h"

bool message_published;

void on_publish(struct mosquitto *mosq, void *userdata, int mid) {
    message_published = true;
}

// 测试 Qo0 级别消息发布
TEST(MQTT_PUBLISH_TEST, public_qos0_topic) {
    mosquitto_lib_init();

    message_published = false;

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        ASSERT_TRUE(false) << "创建 mosquitto 示例失败";
    }

    mosquitto_publish_callback_set(mosq, on_publish);

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        ASSERT_TRUE(false) << "连接 MQTT broker 失败";
    }

    // 发布消息
    const char *topic = "test";
    const char *payload = "Hello, MQTT!";
    int payloadlen = strlen(payload);
    int qos = 0;
    bool retain = false;

    rc = mosquitto_publish(mosq, NULL, topic, payloadlen, payload, qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        ASSERT_TRUE(false) << "消息发布失败";
    }

    // 等待消息发布完成
    while (!message_published) {
        mosquitto_loop(mosq, 0, 1);
    }

    // 断开连接并清理资源
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

// 测试 Qo1 级别消息发布
TEST(MQTT_PUBLISH_TEST, public_qos1_topic) {
    mosquitto_lib_init();

    message_published = false;

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        ASSERT_TRUE(false) << "创建 mosquitto 示例失败";
    }

    mosquitto_publish_callback_set(mosq, on_publish);

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        ASSERT_TRUE(false) << "连接 MQTT broker 失败";
    }

    // 发布消息
    const char *topic = "test";
    const char *payload = "Hello, MQTT!";
    int payloadlen = strlen(payload);
    int qos = 1;
    bool retain = false;

    rc = mosquitto_publish(mosq, NULL, topic, payloadlen, payload, qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        ASSERT_TRUE(false) << "消息发布失败";
    }

    // 等待消息发布完成
    while (!message_published) {
        mosquitto_loop(mosq, 0, 1);
    }

    // 断开连接并清理资源
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

// 测试 Qo2 级别消息发布
TEST(MQTT_PUBLISH_TEST, public_qos2_topic) {
    mosquitto_lib_init();

    message_published = false;

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        ASSERT_TRUE(false) << "创建 mosquitto 示例失败";
    }

    mosquitto_publish_callback_set(mosq, on_publish);

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        ASSERT_TRUE(false) << "连接 MQTT broker 失败";
    }

    // 发布消息
    const char *topic = "test";
    const char *payload = "Hello, MQTT!";
    int payloadlen = strlen(payload);
    int qos = 2;
    bool retain = false;

    rc = mosquitto_publish(mosq, NULL, topic, payloadlen, payload, qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        ASSERT_TRUE(false) << "消息发布失败";
    }

    // 等待消息发布完成
    while (!message_published) {
        mosquitto_loop(mosq, 0, 1);
    }

    // 断开连接并清理资源
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}


int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}