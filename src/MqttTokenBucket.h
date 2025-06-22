#pragma once

#include <chrono>
#include <algorithm>

class MqttTokenBucket {
public:
    MqttTokenBucket(double tokensPerSecond);

    MqttTokenBucket(double tokensPerSecond, double maxTokens);
    
    ~MqttTokenBucket() = default;

    // 尝试消耗一个令牌
    bool tryConsume() noexcept;

private:
    // 补充令牌，按速率计算增加的令牌数量
    void refillTokens() noexcept;

private:
    double tokensPerSecond_;    // 每秒生成的令牌数
    double maxTokens_;          // 桶中最大令牌数
    double tokens_;             // 当前桶中的令牌数
    std::chrono::steady_clock::time_point lastRefillTime_;  // 上次补充令牌的时间
};