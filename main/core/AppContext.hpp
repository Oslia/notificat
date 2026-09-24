#pragma once

#include "core/SystemEvent.hpp"

class AlarmService;
class NotificationService;
class SettingsService;
class TimeService;
class WeatherService;
class WifiService;
class SystemMQTT;

struct AppContext {
    /* 所有権は SystemManager にあり、各 App は参照だけを保持する。 */
    AlarmService &alarms;
    NotificationService &notifications;
    SettingsService &settings;
    TimeService &time;
    WeatherService &weather;
    WifiService &wifi;
    SystemEventBus &events;
    SystemMQTT &mqtt;
};
