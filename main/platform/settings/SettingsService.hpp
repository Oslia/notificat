#pragma once

#include <cstdint>

#include "core/SystemEvent.hpp"
#include "esp_err.h"

struct RegionInfo {
    const char *name;
    const char *timezone;
    double latitude;
    double longitude;
};

class SettingsService {
public:
    static SettingsService &instance();

    void init(SystemEventBus &events);
    uint8_t region_count() const;
    uint8_t selected_region() const;
    const RegionInfo &region(uint8_t index) const;
    const RegionInfo &region() const;
    esp_err_t set_region(uint8_t index);

private:
    SettingsService() = default;

    SystemEventBus *event_bus_ = nullptr;
    bool initialized_ = false;
    uint8_t selected_region_ = 0;
};
