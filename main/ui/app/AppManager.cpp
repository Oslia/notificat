#include "ui/app/AppManager.hpp"

void AppManager::init(AppContext &context)
{
    if (initialized_) {
        return;
    }
    initialized_ = true;

    home_app_.init(context);
    alarm_app_.init(context);
    weather_app_.init(context);
    device_settings_app_.init(context);

    registry_.add(AppId::Home, "Home", home_app_);
    registry_.add(AppId::Alarm, "Alarm", alarm_app_);
    registry_.add(AppId::Weather, "Weather", weather_app_);
    registry_.add(AppId::DeviceSettings, "Settings", device_settings_app_);
}

void AppManager::activate(AppId id, lv_obj_t *content)
{
    if (active_app_ != nullptr) {
        active_app_->onLeave();
    }

    lv_obj_clean(content);
    active_app_ = registry_.find(id);
    if (active_app_ != nullptr) {
        active_app_->onEnter(content);
    }
}

void AppManager::dispatchSystemEvents(SystemEventMask events)
{
    if (active_app_ != nullptr) {
        active_app_->onSystemEvents(events);
    }
}

const AppRegistry &AppManager::registry() const
{
    return registry_;
}
