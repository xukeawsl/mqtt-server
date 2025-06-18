#pragma once

template <typename T>
class MqttSingleton {
public:
    static T* getInstance() {
        static T instance;
        return &instance;
    }

    MqttSingleton(const MqttSingleton&) = delete;
    MqttSingleton& operator=(const MqttSingleton&) = delete;
    MqttSingleton(MqttSingleton&&) = delete;
    MqttSingleton& operator=(MqttSingleton&&) = delete;

protected:
    MqttSingleton() = default;
    virtual ~MqttSingleton() = default;
};