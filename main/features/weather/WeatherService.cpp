#include "features/weather/WeatherService.hpp"

#include <cstring>
#include <ctime>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "platform/network/WifiService.hpp"

namespace {

constexpr TickType_t kRefreshInterval = pdMS_TO_TICKS(60 * 60 * 1000);
constexpr TickType_t kErrorRetryInterval = pdMS_TO_TICKS(5 * 60 * 1000);
constexpr uint32_t kWeatherTaskStackSize = 8192;

void copy_text(char *destination, size_t destination_size, const char *source)
{
    std::strncpy(destination, source != nullptr ? source : "", destination_size - 1);
    destination[destination_size - 1] = '\0';
}

} // namespace

WeatherService &WeatherService::instance()
{
    static WeatherService service;
    return service;
}

void WeatherService::init(const RegionInfo &region, WeatherProvider &provider,
                          WifiService &wifi, SystemEventBus &events)
{
    if (task_ != nullptr) {
        return;
    }

    event_bus_ = &events;
    provider_ = &provider;
    wifi_ = &wifi;
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        snapshot_.status = WeatherStatus::Error;
        snapshot_.last_error = ESP_ERR_NO_MEM;
        publish_changed();
        return;
    }

    set_region(region);
    TaskHandle_t task_handle = nullptr;
    if (xTaskCreate(task_entry, "weather", kWeatherTaskStackSize, this, 3, &task_handle) != pdPASS) {
        set_status(WeatherStatus::Error, ESP_ERR_NO_MEM);
        return;
    }
    task_ = task_handle;
}

void WeatherService::set_region(const RegionInfo &region)
{
    if (mutex_ == nullptr) {
        return;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    location_.latitude = region.latitude;
    location_.longitude = region.longitude;
    ++location_generation_;
    snapshot_.status = WeatherStatus::NotLoaded;
    snapshot_.last_error = ESP_OK;
    snapshot_.updated_at = 0;
    snapshot_.forecast = WeatherForecastData{};
    copy_text(snapshot_.region_name, sizeof(snapshot_.region_name), region.name);
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();
    request_refresh();
}

void WeatherService::request_refresh()
{
    if (task_ != nullptr) {
        xTaskNotifyGive(static_cast<TaskHandle_t>(task_));
    }
}

WeatherSnapshot WeatherService::snapshot() const
{
    if (mutex_ == nullptr) {
        return snapshot_;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const WeatherSnapshot result = snapshot_;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return result;
}

void WeatherService::task_entry(void *context)
{
    static_cast<WeatherService *>(context)->run();
}

void WeatherService::run()
{
    TickType_t wait_time = 0;
    while (true) {
        if (wait_time != 0) {
            ulTaskNotifyTake(pdTRUE, wait_time);
        }

        WifiSnapshot wifi_snapshot{};
        wifi_->snapshot(wifi_snapshot);
        if (wifi_snapshot.connection_state != WifiConnectionState::Connected) {
            set_status(WeatherStatus::WaitingForNetwork);
            wait_time = portMAX_DELAY;
            continue;
        }

        WeatherLocation requested_location{};
        uint32_t requested_generation = 0;
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        requested_location = location_;
        requested_generation = location_generation_;
        snapshot_.status = WeatherStatus::Loading;
        snapshot_.last_error = ESP_OK;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();

        WeatherForecastData forecast{};
        const esp_err_t result = provider_->fetch(requested_location, forecast);

        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        if (requested_generation != location_generation_) {
            xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
            wait_time = 0;
            continue;
        }

        if (result == ESP_OK) {
            snapshot_.forecast = forecast;
            snapshot_.updated_at = static_cast<int64_t>(std::time(nullptr));
            snapshot_.status = WeatherStatus::Available;
            snapshot_.last_error = ESP_OK;
            wait_time = kRefreshInterval;
        } else {
            snapshot_.status = WeatherStatus::Error;
            snapshot_.last_error = result;
            wait_time = kErrorRetryInterval;
        }
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        publish_changed();
    }
}

void WeatherService::publish_changed()
{
    if (event_bus_ != nullptr) {
        event_bus_->publish(SystemEvent::WeatherChanged);
    }
}

void WeatherService::set_status(WeatherStatus status, esp_err_t error)
{
    if (mutex_ == nullptr) {
        snapshot_.status = status;
        snapshot_.last_error = error;
    } else {
        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        snapshot_.status = status;
        snapshot_.last_error = error;
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    }
    publish_changed();
}
