#pragma once

#include <cstdint>

#include "core/AppContext.hpp"
#include "esp_err.h"

enum class SystemManagerState : uint8_t {
    NotInitialized,
    Initializing,
    Ready,
    Error,
};

class SystemManager {
public:
    static SystemManager &instance();

    esp_err_t init();
    SystemManagerState state() const;
    AppContext &context();

private:
    SystemManager();
    static void wifi_connected(void *context);

    SystemEventBus event_bus_;
    AppContext context_;
    SystemManagerState state_ = SystemManagerState::NotInitialized;
    esp_err_t last_error_ = ESP_OK;
};
