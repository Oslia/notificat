#include "platform/settings/SettingsService.hpp"

#include "nvs.h"

namespace {

constexpr char kSettingsNamespace[] = "system";
constexpr char kRegionKey[] = "region";

constexpr RegionInfo kRegions[] = {
    {"Tokyo", "JST-9", 35.6762, 139.6503},
    {"Osaka", "JST-9", 34.6937, 135.5023},
    {"Sapporo", "JST-9", 43.0618, 141.3545},
    {"Fukuoka", "JST-9", 33.5902, 130.4017},
    {"Seoul", "KST-9", 37.5665, 126.9780},
};

constexpr uint8_t kRegionCount = sizeof(kRegions) / sizeof(kRegions[0]);

} // namespace

SettingsService &SettingsService::instance()
{
    static SettingsService service;
    return service;
}

void SettingsService::init(SystemEventBus &events)
{
    event_bus_ = &events;
    if (initialized_) {
        return;
    }

    nvs_handle_t handle;
    if (nvs_open(kSettingsNamespace, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t saved_region = 0;
        if (nvs_get_u8(handle, kRegionKey, &saved_region) == ESP_OK && saved_region < kRegionCount) {
            selected_region_ = saved_region;
        }
        nvs_close(handle);
    }
    initialized_ = true;
}

uint8_t SettingsService::region_count() const
{
    return kRegionCount;
}

uint8_t SettingsService::selected_region() const
{
    return selected_region_;
}

const RegionInfo &SettingsService::region(uint8_t index) const
{
    return kRegions[index < kRegionCount ? index : 0];
}

const RegionInfo &SettingsService::region() const
{
    return region(selected_region_);
}

esp_err_t SettingsService::set_region(uint8_t index)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (index >= kRegionCount) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t result = nvs_open(kSettingsNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_set_u8(handle, kRegionKey, index);
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);

    if (result == ESP_OK) {
        selected_region_ = index;
        if (event_bus_ != nullptr) {
            event_bus_->publish(SystemEvent::RegionChanged);
        }
    }
    return result;
}
