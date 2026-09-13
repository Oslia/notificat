#pragma once

#include <cstdint>

#include "core/AppContext.hpp"
#include "features/alarm/ui/AlarmApp.hpp"
#include "features/home/ui/HomeApp.hpp"
#include "features/settings/ui/DeviceSettingsApp.hpp"
#include "features/weather/ui/WeatherApp.hpp"
#include "ui/app/AppRegistry.hpp"

class AppManager {
public:
    void init(AppContext &context);
    void activate(AppId id, lv_obj_t *content);
    void dispatchSystemEvents(SystemEventMask events);
    const AppRegistry &registry() const;

private:
    bool initialized_ = false;
    App *active_app_ = nullptr;
    AppRegistry registry_;
    HomeApp home_app_;
    AlarmApp alarm_app_;
    WeatherApp weather_app_;
    DeviceSettingsApp device_settings_app_;
};
