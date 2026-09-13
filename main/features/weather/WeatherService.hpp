#pragma once

#include <cstdint>

#include "core/SystemEvent.hpp"
#include "esp_err.h"
#include "features/weather/WeatherProvider.hpp"
#include "platform/settings/SettingsService.hpp"

class WifiService;

enum class WeatherStatus : uint8_t {
    NotLoaded,
    WaitingForNetwork,
    Loading,
    Available,
    Error,
};

struct WeatherSnapshot {
    WeatherStatus status = WeatherStatus::NotLoaded;
    esp_err_t last_error = ESP_OK;
    char region_name[24]{};
    int64_t updated_at = 0;
    WeatherForecastData forecast{};
};

class WeatherService {
public:
    static WeatherService &instance();

    void init(const RegionInfo &region, WeatherProvider &provider,
              WifiService &wifi, SystemEventBus &events);
    void set_region(const RegionInfo &region);
    void request_refresh();
    WeatherSnapshot snapshot() const;

private:
    WeatherService() = default;

    static void task_entry(void *context);
    void run();
    void publish_changed();
    void set_status(WeatherStatus status, esp_err_t error = ESP_OK);

    SystemEventBus *event_bus_ = nullptr;
    WeatherProvider *provider_ = nullptr;
    WifiService *wifi_ = nullptr;
    void *mutex_ = nullptr;
    void *task_ = nullptr;
    WeatherSnapshot snapshot_{};
    WeatherLocation location_{};
    uint32_t location_generation_ = 0;
};
