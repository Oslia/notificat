#include "core/SystemManager.hpp"

#include "esp_log.h"
#include "features/alarm/AlarmService.hpp"
#include "features/notification/NotificationService.hpp"
#include "features/weather/WeatherService.hpp"
#include "features/weather/providers/OpenMeteoProvider.hpp"
#include "platform/network/WifiService.hpp"
#include "platform/audio/CoreS3AlarmOutput.hpp"
#include "platform/settings/SettingsService.hpp"
#include "platform/time/TimeService.hpp"

namespace {

constexpr char kLogTag[] = "SystemManager";

} // namespace

SystemManager &SystemManager::instance()
{
    static SystemManager manager;
    return manager;
}

SystemManager::SystemManager()
    : context_{AlarmService::instance(), NotificationService::instance(),
               SettingsService::instance(), TimeService::instance(),
               WeatherService::instance(),
               WifiService::instance(), event_bus_}
{
}

esp_err_t SystemManager::init()
{
    if (state_ == SystemManagerState::Ready || state_ == SystemManagerState::Error) {
        return last_error_;
    }
    if (state_ == SystemManagerState::Initializing) {
        return ESP_ERR_INVALID_STATE;
    }
    state_ = SystemManagerState::Initializing;

    SettingsService &settings = context_.settings;
    settings.init(context_.events);
    context_.notifications.init(context_.events);
    context_.time.init(settings.region().timezone, context_.events);
    context_.alarms.init(context_.time, context_.notifications,
                         CoreS3AlarmOutput::instance(), context_.events);
    context_.weather.init(settings.region(), OpenMeteoProvider::instance(),
                          context_.wifi, context_.events);
    /* ドライバ初期化だけをここで行い、接続処理と後続イベントは非同期で進める。 */
    context_.wifi.set_connected_callback(wifi_connected, this);
    last_error_ = context_.wifi.init(context_.events);
    if (last_error_ != ESP_OK) {
        state_ = SystemManagerState::Error;
        ESP_LOGE(kLogTag, "Wi-Fi initialization failed: %s", esp_err_to_name(last_error_));
        return last_error_;
    }

    state_ = SystemManagerState::Ready;
    return ESP_OK;
}

SystemManagerState SystemManager::state() const
{
    return state_;
}

AppContext &SystemManager::context()
{
    return context_;
}

void SystemManager::wifi_connected(void *context)
{
    auto *manager = static_cast<SystemManager *>(context);
    manager->context_.time.start_time_sync();
    manager->context_.weather.request_refresh();
    manager->context_.events.publish(SystemEvent::TimeChanged);
}
