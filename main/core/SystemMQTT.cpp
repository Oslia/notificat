#include "core/SystemMQTT.hpp"
#include "core/MqttRouting.hpp"
#include "sdkconfig.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <new>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "mqtt_client.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "platform/network/WifiService.hpp"
#include "platform/time/TimeService.hpp"

#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
namespace {
constexpr char kTag[] = "SystemMQTT";
// ESP-MQTT retains the PEM pointers for reconnects; keep them for its lifetime.
char *certificate = nullptr;
char *private_key = nullptr;
char *root_ca = nullptr;


void log_heap(const char *stage)
{
    constexpr uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    constexpr uint32_t external = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    ESP_LOGI(kTag, "%s: internal free=%u largest=%u minimum=%u; PSRAM free=%u",
             stage,
             static_cast<unsigned>(heap_caps_get_free_size(internal)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(internal)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(internal)),
             static_cast<unsigned>(heap_caps_get_free_size(external)));
}

bool log_connection_identity()
{
    /* 実機の NVS から読み込んだ証明書を識別する。秘密鍵はログに出さない。 */
    mbedtls_x509_crt parsed;
    mbedtls_x509_crt_init(&parsed);
    int result = mbedtls_x509_crt_parse(&parsed,
        reinterpret_cast<const unsigned char *>(certificate), std::strlen(certificate) + 1);
    unsigned char digest[32]{};
    if (result == 0) result = mbedtls_sha256(parsed.raw.p, parsed.raw.len, digest, 0);
    mbedtls_x509_crt_free(&parsed);
    if (result != 0) {
        ESP_LOGE(kTag, "Cannot fingerprint device certificate: %d", result);
        return false;
    }
    constexpr char hex[] = "0123456789abcdef";
    char fingerprint[65]{};
    for (size_t i = 0; i < sizeof(digest); ++i) {
        fingerprint[i * 2] = hex[digest[i] >> 4];
        fingerprint[i * 2 + 1] = hex[digest[i] & 15];
    }
    ESP_LOGI(kTag, "Endpoint=%s:8883 client_id=%s nvs=%s",
             CONFIG_NOTIFICAT_MQTT_ENDPOINT, CONFIG_NOTIFICAT_MQTT_CLIENT_ID,
             CONFIG_NOTIFICAT_MQTT_NVS_PARTITION);
    ESP_LOGI(kTag, "Device certificate SHA256=%s", fingerprint);
    return true;
}

esp_err_t read_pem(nvs_handle_t nvs, const char *key, char **output)
{
    size_t size = 0;
    esp_err_t err = nvs_get_str(nvs, key, nullptr, &size);
    if (err != ESP_OK) return err;
    if (size < 2 || size > 16384) return ESP_ERR_INVALID_SIZE;
    /* 再接続まで保持する PEM は PSRAM を優先し、内部 RAM をタスク用に残す。 */
    *output = static_cast<char *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (*output == nullptr) {
        *output = static_cast<char *>(heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (*output == nullptr) return ESP_ERR_NO_MEM;
    return nvs_get_str(nvs, key, *output, &size);
}

void release_credentials()
{
    std::free(certificate);
    std::free(private_key);
    std::free(root_ca);
    certificate = private_key = root_ca = nullptr;
}

} // namespace

struct SystemMQTT::Impl {
    static constexpr size_t QueueDepth = 4;
    struct Listener {
        Subscription id = 0;
        char topic[MaximumTopicBytes + 1]{};
        Callback callback = nullptr;
        void *context = nullptr;
        bool ready = false;
    } listeners[MaximumSubscriptions];
    struct BrokerTopic {
        char topic[MaximumTopicBytes + 1]{};
        int pending = -1;
        bool ready = false;
        TickType_t retry_at = 0;
    } broker[MaximumSubscriptions];
    struct Outgoing {
        char topic[MaximumTopicBytes + 1];
        uint8_t payload[MaximumPayloadBytes];
        size_t size;
        int qos;
        bool retain;
        uint32_t epoch;
    } outgoing{};
    struct Ack { int id; bool accepted; uint32_t epoch; };
    // 大きいバッファとキュー領域は Impl と共に PSRAM に置く。
    uint8_t rx_storage[QueueDepth * sizeof(mqtt_detail::Frame)];
    uint8_t tx_storage[QueueDepth * sizeof(Outgoing)];
    uint8_t ack_storage[MaximumSubscriptions * sizeof(Ack)];
    QueueHandle_t rx = nullptr, tx = nullptr, acks = nullptr;
    SemaphoreHandle_t mutex = nullptr;
    mqtt_detail::Assembly assembly;
    mqtt_detail::Frame delivery;
    esp_mqtt_client_handle_t client = nullptr;
    std::atomic<bool> connected{false};
    std::atomic<uint32_t> epoch{0};
    Subscription next_id = 1;
    bool dispatching = false;

    void lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }
    void unlock() { xSemaphoreGiveRecursive(mutex); }

    static void event(void *context, esp_event_base_t, int32_t id, void *data)
    {
        auto &self = *static_cast<Impl *>(context);
        const auto &e = *static_cast<esp_mqtt_event_t *>(data);
        if (id == MQTT_EVENT_CONNECTED || id == MQTT_EVENT_DISCONNECTED) {
            self.assembly.reset();
            self.lock();
            for (auto &listener : self.listeners) listener.ready = false;
            self.epoch.fetch_add(1);
            self.connected.store(id == MQTT_EVENT_CONNECTED);
            self.unlock();
            ESP_LOGI(kTag, "%s", id == MQTT_EVENT_CONNECTED ? "CONNECTED" : "DISCONNECTED");
        } else if (id == MQTT_EVENT_SUBSCRIBED) {
            Ack ack{e.msg_id, e.data && e.data_len == 1 &&
                static_cast<uint8_t>(e.data[0]) <= 1, self.epoch.load()};
            if (xQueueSend(self.acks, &ack, 0) != pdTRUE) ESP_LOGW(kTag, "SUBACK queue full");
        } else if (id == MQTT_EVENT_PUBLISHED) {
            ESP_LOGI(kTag, "PUBACK id=%d", e.msg_id);
        } else if (id == MQTT_EVENT_DATA) {
            if (e.topic_len < 0 || e.data_len < 0 || e.current_data_offset < 0 || e.total_data_len < 0) return;
            if (e.current_data_offset == 0) {
                self.assembly.frame.recipients.fill(0);
                self.lock();
                for (size_t i = 0; i < MaximumSubscriptions; ++i) {
                    auto &l = self.listeners[i];
                    if (l.id && e.topic && std::strlen(l.topic) == static_cast<size_t>(e.topic_len) &&
                        std::memcmp(l.topic, e.topic, e.topic_len) == 0)
                        self.assembly.frame.recipients[i] = l.id;
                }
                self.unlock();
                if (e.total_data_len > static_cast<int>(MaximumPayloadBytes))
                    ESP_LOGW(kTag, "Dropping oversized MQTT payload: %d", e.total_data_len);
            }
            if (self.assembly.append(e.topic, e.topic_len, e.data, e.data_len,
                                     e.current_data_offset, e.total_data_len)) {
                if (xQueueSend(self.rx, &self.assembly.frame, 0) != pdTRUE)
                    ESP_LOGW(kTag, "RX queue full; dropping message");
            }
        } else if (id == MQTT_EVENT_ERROR && e.error_handle) {
            const auto &error = *e.error_handle;
            ESP_LOGE(kTag, "ERROR type=%d tls=0x%x stack=0x%x socket=%d", error.error_type,
                     error.esp_tls_last_esp_err, error.esp_tls_stack_err, error.esp_transport_sock_errno);
            if (error.error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
                ESP_LOGE(kTag, "CONNACK refusal code=%d", error.connect_return_code);
        }
    }

    void reconcile(uint32_t generation)
    {
        // ネットワーク API の呼び出し中は登録表の mutex を保持しない。
        for (auto &b : broker) {
            if (!b.topic[0]) continue;
            bool wanted = false;
            lock();
            for (const auto &l : listeners) if (l.id && std::strcmp(l.topic, b.topic) == 0) wanted = true;
            unlock();
            if (!wanted) {
                if (esp_mqtt_client_unsubscribe(client, b.topic) >= 0) b = {};
            }
        }
        for (size_t i = 0; i < MaximumSubscriptions; ++i) {
            char topic[MaximumTopicBytes + 1]{};
            lock();
            if (listeners[i].id) std::strcpy(topic, listeners[i].topic);
            unlock();
            if (!topic[0]) continue;
            BrokerTopic *found = nullptr, *empty = nullptr;
            for (auto &b : broker) {
                if (!b.topic[0]) empty = &b;
                else if (std::strcmp(b.topic, topic) == 0) found = &b;
            }
            if (!found && empty) { found = empty; std::strcpy(found->topic, topic); }
            if (!found) continue;
            if (!found->ready && (found->retry_at == 0 ||
                static_cast<int32_t>(xTaskGetTickCount() - found->retry_at) >= 0)) {
                found->pending = esp_mqtt_client_subscribe(client, topic, 1);
                found->retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
            }
            lock();
            if (connected.load() && generation == epoch.load()) {
                for (auto &l : listeners)
                    if (l.id && std::strcmp(l.topic, topic) == 0) l.ready = found->ready;
            }
            unlock();
        }
    }

    static void worker(void *context)
    {
        auto &self = *static_cast<Impl *>(context);
        ESP_LOGI(kTag, "Waiting for Wi-Fi and SNTP");
        WifiSnapshot wifi{};
        do {
            vTaskDelay(pdMS_TO_TICKS(1000));
            WifiService::instance().snapshot(wifi);
        } while (wifi.connection_state != WifiConnectionState::Connected ||
                 TimeService::instance().sync_state() != TimeSyncState::Synchronized);
        const char *partition = CONFIG_NOTIFICAT_MQTT_NVS_PARTITION;
        esp_err_t err = nvs_flash_init_partition(partition);
        nvs_handle_t nvs = 0;
        if (err == ESP_OK) err = nvs_open_from_partition(partition, "cert_key", NVS_READONLY, &nvs);
        if (err == ESP_OK) {
            err = read_pem(nvs, "cert", &certificate);
            if (err == ESP_OK) err = read_pem(nvs, "key", &private_key);
            if (err == ESP_OK) err = read_pem(nvs, "ca1", &root_ca);
            nvs_close(nvs);
        }
        if (err == ESP_OK && !log_connection_identity()) err = ESP_FAIL;
        if (err == ESP_OK) {
            esp_mqtt_client_config_t config{};
            config.broker.address.hostname = CONFIG_NOTIFICAT_MQTT_ENDPOINT;
            config.broker.address.port = 8883;
            config.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
            config.broker.verification.certificate = root_ca;
            config.credentials.client_id = CONFIG_NOTIFICAT_MQTT_CLIENT_ID;
            config.credentials.authentication.certificate = certificate;
            config.credentials.authentication.key = private_key;
            config.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
            config.network.reconnect_timeout_ms = 10000;
            config.outbox.limit = 8192;
            log_heap("Before MQTT client init");
            self.client = esp_mqtt_client_init(&config);
            err = self.client ? esp_mqtt_client_register_event(self.client, MQTT_EVENT_ANY, event, &self) : ESP_ERR_NO_MEM;
        }
        if (err == ESP_OK) {
            for (int attempt = 1; attempt <= 6; ++attempt) {
                log_heap("Before MQTT task start");
                err = esp_mqtt_client_start(self.client);
                if (err == ESP_OK || err != ESP_FAIL || attempt == 6) break;
                ESP_LOGW(kTag, "MQTT task start failed; retry %d/6 in 5s", attempt);
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "MQTT initialization failed: %s", esp_err_to_name(err));
            log_heap("MQTT initialization failed");
            if (self.client) esp_mqtt_client_destroy(self.client);
            self.client = nullptr;
            release_credentials();
            vTaskDelete(nullptr);
            return;
        }
        uint32_t generation = UINT32_MAX;
        for (;;) {
            const uint32_t current = self.epoch.load();
            if (generation != current) {
                generation = current;
                for (auto &b : self.broker) b = {};
            }
            Ack ack{};
            while (xQueueReceive(self.acks, &ack, 0) == pdTRUE) {
                if (ack.epoch != generation) continue;
                for (auto &b : self.broker) if (b.topic[0] && b.pending == ack.id) {
                    b.ready = ack.accepted;
                    b.pending = -1;
                    ESP_LOGI(kTag, "SUBACK topic=%s accepted=%d", b.topic, ack.accepted);
                }
            }
            if (self.connected.load()) self.reconcile(generation);
            // 切断をまたいだ未送信コマンドは再接続後に遅れて実行しない。
            for (size_t i = 0; i < QueueDepth && xQueueReceive(self.tx, &self.outgoing, 0) == pdTRUE; ++i) {
                if (!self.connected.load() || self.outgoing.epoch != self.epoch.load()) { ESP_LOGW(kTag, "Dropping publish after disconnect"); continue; }
                const auto &out = self.outgoing;
                const int id = esp_mqtt_client_enqueue(self.client, out.topic,
                    out.size ? reinterpret_cast<const char *>(out.payload) : "",
                    out.size, out.qos, out.retain, true);
                if (id < 0) ESP_LOGW(kTag, "Publish rejected by outbox: %d", id);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
};
#endif

SystemMQTT &SystemMQTT::instance()
{
    static SystemMQTT service;
    return service;
}

esp_err_t SystemMQTT::init()
{
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    if (impl_) return ESP_OK;
    if (!CONFIG_NOTIFICAT_MQTT_ENDPOINT[0] || std::strpbrk(CONFIG_NOTIFICAT_MQTT_ENDPOINT, "/:") ||
        !CONFIG_NOTIFICAT_MQTT_CLIENT_ID[0]) return ESP_ERR_INVALID_ARG;
    void *memory = heap_caps_malloc(sizeof(Impl), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) return ESP_ERR_NO_MEM;
    impl_ = new (memory) Impl{};
    static StaticQueue_t rx_control{}, tx_control{}, ack_control{};
    impl_->mutex = xSemaphoreCreateRecursiveMutex();
    impl_->rx = xQueueCreateStatic(Impl::QueueDepth, sizeof(mqtt_detail::Frame), impl_->rx_storage, &rx_control);
    impl_->tx = xQueueCreateStatic(Impl::QueueDepth, sizeof(Impl::Outgoing), impl_->tx_storage, &tx_control);
    impl_->acks = xQueueCreateStatic(MaximumSubscriptions, sizeof(Impl::Ack), impl_->ack_storage, &ack_control);
    if (!impl_->mutex || !impl_->rx || !impl_->tx || !impl_->acks ||
        xTaskCreate(Impl::worker, "mqtt_service", 4096, impl_, 3, nullptr) != pdPASS) {
        if (impl_->mutex) vSemaphoreDelete(impl_->mutex);
        if (impl_->rx) vQueueDelete(impl_->rx);
        if (impl_->tx) vQueueDelete(impl_->tx);
        if (impl_->acks) vQueueDelete(impl_->acks);
        impl_->~Impl();
        heap_caps_free(impl_);
        impl_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t SystemMQTT::subscribe(const char *topic, Callback callback, void *context, Subscription &handle)
{
    handle = 0;
    if (!mqtt_detail::valid_topic(topic) || !callback) return ESP_ERR_INVALID_ARG;
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    if (!impl_) return ESP_ERR_INVALID_STATE;
    impl_->lock();
    for (auto &l : impl_->listeners) if (!l.id && impl_->next_id != 0) {
        l = {};
        l.id = impl_->next_id++;
        std::strcpy(l.topic, topic);
        l.callback = callback;
        l.context = context;
        handle = l.id;
        impl_->unlock();
        return ESP_OK;
    }
    impl_->unlock();
    return ESP_ERR_NO_MEM;
#else
    (void)context;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t SystemMQTT::unsubscribe(Subscription handle)
{
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    if (!impl_ || !handle) return ESP_ERR_INVALID_ARG;
    impl_->lock();
    for (auto &l : impl_->listeners) if (l.id == handle) {
        l = {};
        impl_->unlock();
        return ESP_OK;
    }
    impl_->unlock();
    return ESP_ERR_NOT_FOUND;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

bool SystemMQTT::connected() const
{
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    return impl_ && impl_->connected.load();
#else
    return false;
#endif
}

bool SystemMQTT::subscribed(Subscription handle)
{
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    if (!impl_) return false;
    impl_->lock();
    bool ready = false;
    for (const auto &l : impl_->listeners) if (l.id && l.id == handle) ready = l.ready;
    impl_->unlock();
    return ready;
#else
    (void)handle;
    return false;
#endif
}

uint32_t SystemMQTT::connection_generation() const
{
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    return impl_ ? impl_->epoch.load() : 0;
#else
    return 0;
#endif
}

esp_err_t SystemMQTT::publish(const char *topic, const void *payload, size_t size, int qos, bool retain)
{
    if (!mqtt_detail::valid_topic(topic) || size > MaximumPayloadBytes || (size && !payload) ||
        (qos != 0 && qos != 1)) return ESP_ERR_INVALID_ARG;
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    if (!impl_) return ESP_ERR_INVALID_STATE;
    const uint32_t generation = impl_->epoch.load();
    if (!connected()) return ESP_ERR_INVALID_STATE;
    // 呼び出し元スタックを圧迫しないよう、一時メッセージも PSRAM に確保する。
    auto *out = static_cast<Impl::Outgoing *>(heap_caps_malloc(sizeof(Impl::Outgoing), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!out) return ESP_ERR_NO_MEM;
    std::strcpy(out->topic, topic);
    if (size) std::memcpy(out->payload, payload, size);
    out->epoch = generation;
    out->size = size; out->qos = qos; out->retain = retain;
    const bool queued = xQueueSend(impl_->tx, out, 0) == pdTRUE;
    heap_caps_free(out);
    return queued ? ESP_OK : ESP_ERR_NO_MEM;
#else
    (void)retain;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

void SystemMQTT::dispatch()
{
#ifdef CONFIG_NOTIFICAT_MQTT_ENABLED
    if (!impl_) return;
    impl_->lock();
    if (impl_->dispatching) { impl_->unlock(); return; }
    impl_->dispatching = true;
    // コールバックは LVGL タイマー上で実行し、解除後の古い受信データを捨てる。
    for (size_t n = 0; n < Impl::QueueDepth && xQueueReceive(impl_->rx, &impl_->delivery, 0) == pdTRUE; ++n) {
        auto &frame = impl_->delivery;
        for (size_t i = 0; i < MaximumSubscriptions; ++i) {
            auto &l = impl_->listeners[i];
            if (l.id && l.id == frame.recipients[i]) {
                auto callback = l.callback;
                void *context = l.context;
                callback({frame.topic, frame.payload, frame.size}, context);
            }
        }
    }
    impl_->dispatching = false;
    impl_->unlock();
#endif
}
