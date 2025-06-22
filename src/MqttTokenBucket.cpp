#include "MqttTokenBucket.h"

MqttTokenBucket::MqttTokenBucket(double tokensPerSecond)
    : tokensPerSecond_(tokensPerSecond),
      maxTokens_(tokensPerSecond),
      tokens_(tokensPerSecond),
      lastRefillTime_(std::chrono::steady_clock::now()) {}

MqttTokenBucket::MqttTokenBucket(double tokensPerSecond, double maxTokens)
    : tokensPerSecond_(tokensPerSecond),
      maxTokens_(maxTokens),
      tokens_(maxTokens),
      lastRefillTime_(std::chrono::steady_clock::now()) {}

bool MqttTokenBucket::tryConsume() noexcept {
    refillTokens();    // 在消费之前先补充令牌
    if (tokens_ >= 1.0) {
        tokens_ -= 1.0;
        return true;
    }
    return false;
}

void MqttTokenBucket::refillTokens() noexcept {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - lastRefillTime_;
    double newTokens = elapsed.count() * tokensPerSecond_;    // 按时间补充令牌
    tokens_ = std::min(maxTokens_, tokens_ + newTokens);    // 更新桶中的令牌数
    lastRefillTime_ = now;    // 更新上次补充时间
}