#pragma once

#include <cstdint>

#include "core/SystemEvent.hpp"
#include "esp_err.h"
#include "esp_event.h"

struct WifiNetwork {
    char ssid[33];
    int8_t rssi;
    bool secured;
};

enum class WifiConnectionState : uint8_t {
    Off,
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error,
};

enum class WifiError : uint8_t {
    None,
    ScanStartFailed,
    ConnectStartFailed,
    ConnectionLost,
    DisconnectFailed,
};

struct WifiSnapshot {
    WifiConnectionState connection_state = WifiConnectionState::Off;
    WifiError last_error = WifiError::None;
    bool scan_in_progress = false;
    uint32_t network_generation;
    uint8_t network_count;
    char connected_ssid[33];
    WifiNetwork networks[12];
};

class WifiService {
public:
    using ConnectedCallback = void (*)(void *context);

    static WifiService &instance();

    esp_err_t init(SystemEventBus &events);
    void set_connected_callback(ConnectedCallback callback, void *context);
    esp_err_t scan();
    esp_err_t connect(const char *ssid, const char *password);
    esp_err_t disconnect();
    void snapshot(WifiSnapshot &snapshot);

private:
    WifiService() = default;

    static void event_handler(void *argument, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);
    void handle_event(esp_event_base_t event_base, int32_t event_id);
    void publish_changed();

    SystemEventBus *event_bus_ = nullptr;
    ConnectedCallback connected_callback_ = nullptr;
    void *connected_callback_context_ = nullptr;
    void *mutex_ = nullptr;
    bool initialized_ = false;
    bool ignore_next_disconnect_ = false;
    WifiSnapshot snapshot_{};
};
