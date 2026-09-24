#include "core/SystemMQTT.hpp"
#include "sdkconfig.h"
#include "esp_log.h"

#ifdef CONFIG_NOTIFICAT_MQTT_TEST
namespace {
SystemMQTT::Subscription subscription = 0;
uint32_t generation = UINT32_MAX;
bool sent = false;
void received(const MqttMessage &message, void *)
{
    ESP_LOGI("MQTTTest", "RX topic=%s bytes=%u payload=%.*s", message.topic,
             static_cast<unsigned>(message.size), static_cast<int>(message.size),
             reinterpret_cast<const char *>(message.payload));
}
}
#endif

void system_mqtt_test_init(SystemMQTT &mqtt)
{
#ifdef CONFIG_NOTIFICAT_MQTT_TEST
    const esp_err_t err = mqtt.subscribe(CONFIG_NOTIFICAT_MQTT_TOPIC, received, nullptr, subscription);
    if (err != ESP_OK) ESP_LOGE("MQTTTest", "Subscribe failed: %s", esp_err_to_name(err));
#else
    (void)mqtt;
#endif
}

void system_mqtt_test_poll(SystemMQTT &mqtt)
{
#ifdef CONFIG_NOTIFICAT_MQTT_TEST
    const uint32_t current = mqtt.connection_generation();
    if (generation != current) { generation = current; sent = false; }
    if (!sent && mqtt.subscribed(subscription)) {
        constexpr char payload[] = "{\"message\":\"hello from notificat\"}";
        sent = mqtt.publish(CONFIG_NOTIFICAT_MQTT_TOPIC, payload, sizeof(payload) - 1) == ESP_OK;
    }
#else
    (void)mqtt;
#endif
}
