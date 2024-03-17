#include <iostream>

#include "gtest/gtest.h"
#include "mosquitto.h"

bool message_subscribed;

void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos) {
    message_subscribed = true;
}

// 测试 Qos0 级别主题订阅
TEST(MQTT_SUBSCRIBE_TEST, subscribe_qos0_topic) {
    mosquitto_lib_init();

    message_subscribed = false;

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        ASSERT_TRUE(false) << "创建 mosquitto 示例失败";
    }

    mosquitto_subscribe_callback_set(mosq, on_subscribe);

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        ASSERT_TRUE(false) << "连接 MQTT broker 失败";
    }

    rc = mosquitto_subscribe(mosq, NULL, "test", 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        ASSERT_TRUE(false) << "主题订阅失败";
    }

    // 等待订阅完成
    while (!message_subscribed) {
        mosquitto_loop(mosq, 0, 1);
    }

    // 断开连接并清理资源
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

// 测试 Qos1 级别主题订阅
TEST(MQTT_SUBSCRIBE_TEST, subscribe_qos1_topic) {
    mosquitto_lib_init();

    message_subscribed = false;

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        ASSERT_TRUE(false) << "创建 mosquitto 示例失败";
    }

    mosquitto_subscribe_callback_set(mosq, on_subscribe);

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        ASSERT_TRUE(false) << "连接 MQTT broker 失败";
    }

    rc = mosquitto_subscribe(mosq, NULL, "test", 1);
    if (rc != MOSQ_ERR_SUCCESS) {
        ASSERT_TRUE(false) << "主题订阅失败";
    }

    // 等待订阅完成
    while (!message_subscribed) {
        mosquitto_loop(mosq, 0, 1);
    }

    // 断开连接并清理资源
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

// 测试 Qos2 级别主题订阅
TEST(MQTT_SUBSCRIBE_TEST, subscribe_qos2_topic) {
    mosquitto_lib_init();

    message_subscribed = false;

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        ASSERT_TRUE(false) << "创建 mosquitto 示例失败";
    }

    mosquitto_subscribe_callback_set(mosq, on_subscribe);

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        ASSERT_TRUE(false) << "连接 MQTT broker 失败";
    }

    rc = mosquitto_subscribe(mosq, NULL, "test", 2);
    if (rc != MOSQ_ERR_SUCCESS) {
        ASSERT_TRUE(false) << "主题订阅失败";
    }

    // 等待订阅完成
    while (!message_subscribed) {
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