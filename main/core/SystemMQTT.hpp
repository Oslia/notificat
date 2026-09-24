#pragma once

#include <cstddef>
#include <cstdint>
#include "esp_err.h"

struct MqttMessage {
    const char *topic;
    const uint8_t *payload;
    size_t size;
};

class SystemMQTT {
public:
    using Subscription = uint32_t;
    using Callback = void (*)(const MqttMessage &, void *);
    static constexpr size_t MaximumSubscriptions = 16;
    static constexpr size_t MaximumTopicBytes = 256;
    static constexpr size_t MaximumPayloadBytes = 1024;

    static SystemMQTT &instance();
    esp_err_t init();
    // 完全一致のトピックを QoS 1 で購読。成功時に所有者専用のハンドルを返す。
    esp_err_t subscribe(const char *topic, Callback callback, void *context, Subscription &handle);
    esp_err_t unsubscribe(Subscription handle);
    bool subscribed(Subscription handle);
    bool connected() const;
    uint32_t connection_generation() const;
    // 切断中は拒否。ESP_OK はキューへの受付であり、PUBACK や実行完了ではない。
    esp_err_t publish(const char *topic, const void *payload, size_t size,
                      int qos = 1, bool retain = false);
    // AppShell の LVGL タイマーのみから呼ぶ。コールバックの引数は呼び出し中のみ有効。
    void dispatch();

private:
    SystemMQTT() = default;
    struct Impl;
    Impl *impl_ = nullptr;
};

// 診断機能もアプリと同じ公開 API を利用する。
void system_mqtt_test_init(SystemMQTT &mqtt);
void system_mqtt_test_poll(SystemMQTT &mqtt);
