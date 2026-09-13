#include "platform/network/WifiService.hpp"

#include <algorithm>
#include <cstring>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

constexpr uint16_t kMaxAccessPoints = 12;

void copy_text(char *destination, size_t destination_size, const char *source)
{
    std::strncpy(destination, source != nullptr ? source : "", destination_size - 1);
    destination[destination_size - 1] = '\0';
}

} // namespace

WifiService &WifiService::instance()
{
    static WifiService service;
    return service;
}

void WifiService::set_connected_callback(ConnectedCallback callback, void *context)
{
    connected_callback_ = callback;
    connected_callback_context_ = context;
}

esp_err_t WifiService::init(SystemEventBus &events)
{
    if (initialized_) {
        return ESP_OK;
    }
    event_bus_ = &events;

    if (mutex_ == nullptr) {
        mutex_ = xSemaphoreCreateMutex();
        if (mutex_ == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    if (esp_netif_create_default_wifi_sta() == nullptr) {
        return ESP_FAIL;
    }

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&config);
    if (result != ESP_OK && result != ESP_ERR_WIFI_INIT_STATE) {
        return result;
    }

    result = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &WifiService::event_handler, this, nullptr);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &WifiService::event_handler, this, nullptr);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK) {
        return result;
    }

    result = esp_wifi_start();
    if (result != ESP_OK && result != ESP_ERR_WIFI_CONN) {
        return result;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    initialized_ = true;

    wifi_config_t saved_config{};
    const bool has_saved_network = esp_wifi_get_config(WIFI_IF_STA, &saved_config) == ESP_OK
                                   && saved_config.sta.ssid[0] != '\0';
    snapshot_.connection_state = has_saved_network ? WifiConnectionState::Connecting
                                                   : WifiConnectionState::Disconnected;
    if (has_saved_network) {
        copy_text(snapshot_.connected_ssid, sizeof(snapshot_.connected_ssid),
                  reinterpret_cast<const char *>(saved_config.sta.ssid));
    }
    snapshot_.last_error = WifiError::None;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();

    if (has_saved_network && esp_wifi_connect() != ESP_OK) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        snapshot_.connection_state = WifiConnectionState::Error;
        snapshot_.last_error = WifiError::ConnectStartFailed;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();
    }
    return ESP_OK;
}

esp_err_t WifiService::scan()
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    snapshot_.scan_in_progress = true;
    snapshot_.network_count = 0;
    snapshot_.network_generation++;
    snapshot_.last_error = WifiError::None;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();

    const esp_err_t result = esp_wifi_scan_start(nullptr, false);
    if (result != ESP_OK) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        snapshot_.scan_in_progress = false;
        snapshot_.last_error = WifiError::ScanStartFailed;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();
    }
    return result;
}

esp_err_t WifiService::connect(const char *ssid, const char *password)
{
    if (ssid == nullptr || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t config{};
    copy_text(reinterpret_cast<char *>(config.sta.ssid), sizeof(config.sta.ssid), ssid);
    copy_text(reinterpret_cast<char *>(config.sta.password), sizeof(config.sta.password), password);
    config.sta.scan_method = WIFI_FAST_SCAN;
    config.sta.failure_retry_cnt = 3;

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const bool was_connected = snapshot_.connection_state == WifiConnectionState::Connected;
    ignore_next_disconnect_ = was_connected;
    snapshot_.connection_state = WifiConnectionState::Connecting;
    snapshot_.last_error = WifiError::None;
    copy_text(snapshot_.connected_ssid, sizeof(snapshot_.connected_ssid), ssid);
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();

    if (was_connected) {
        esp_wifi_disconnect();
    }
    esp_err_t result = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (result == ESP_OK) {
        result = esp_wifi_connect();
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    if (result != ESP_OK) {
        snapshot_.connection_state = WifiConnectionState::Error;
        snapshot_.last_error = WifiError::ConnectStartFailed;
    }
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();
    return result;
}

esp_err_t WifiService::disconnect()
{
    if (!initialized_) {
        return ESP_OK;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    snapshot_.connection_state = WifiConnectionState::Disconnecting;
    snapshot_.last_error = WifiError::None;
    ignore_next_disconnect_ = false;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();

    esp_err_t result = esp_wifi_disconnect();
    if (result == ESP_ERR_WIFI_NOT_CONNECT) {
        result = ESP_OK;
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        snapshot_.connection_state = WifiConnectionState::Disconnected;
        snapshot_.connected_ssid[0] = '\0';
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();
    } else if (result != ESP_OK) {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        snapshot_.connection_state = WifiConnectionState::Error;
        snapshot_.last_error = WifiError::DisconnectFailed;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();
    }
    return result;
}

void WifiService::snapshot(WifiSnapshot &snapshot)
{
    if (mutex_ == nullptr) {
        snapshot = {};
        return;
    }
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    snapshot = snapshot_;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void WifiService::event_handler(void *argument, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    (void)event_data;
    static_cast<WifiService *>(argument)->handle_event(event_base, event_id);
}

void WifiService::handle_event(esp_event_base_t event_base, int32_t event_id)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        uint16_t count = kMaxAccessPoints;
        wifi_ap_record_t records[kMaxAccessPoints]{};
        esp_wifi_scan_get_ap_records(&count, records);

        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        snapshot_.network_count = static_cast<uint8_t>(std::min<uint16_t>(count, kMaxAccessPoints));
        for (uint8_t index = 0; index < snapshot_.network_count; ++index) {
            copy_text(snapshot_.networks[index].ssid, sizeof(snapshot_.networks[index].ssid),
                      reinterpret_cast<const char *>(records[index].ssid));
            snapshot_.networks[index].rssi = records[index].rssi;
            snapshot_.networks[index].secured = records[index].authmode != WIFI_AUTH_OPEN;
        }
        snapshot_.scan_in_progress = false;
        snapshot_.network_generation++;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();
        return;
    }

    bool should_start_time_sync = false;
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        snapshot_.connection_state = WifiConnectionState::Connected;
        snapshot_.last_error = WifiError::None;
        should_start_time_sync = true;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (ignore_next_disconnect_) {
            ignore_next_disconnect_ = false;
        } else if (snapshot_.connection_state == WifiConnectionState::Disconnecting) {
            snapshot_.connection_state = WifiConnectionState::Disconnected;
            snapshot_.connected_ssid[0] = '\0';
        } else if (snapshot_.connection_state == WifiConnectionState::Connecting
                   || snapshot_.connection_state == WifiConnectionState::Connected) {
            snapshot_.connection_state = WifiConnectionState::Error;
            snapshot_.last_error = WifiError::ConnectionLost;
        }
    }
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();

    if (should_start_time_sync) {
        if (connected_callback_ != nullptr) {
            connected_callback_(connected_callback_context_);
        }
    }
}

void WifiService::publish_changed()
{
    if (event_bus_ != nullptr) {
        event_bus_->publish(SystemEvent::WifiChanged);
    }
}
