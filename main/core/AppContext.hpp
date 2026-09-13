#pragma once

#include "core/SystemEvent.hpp"

class AlarmService;
class NotificationService;
class SettingsService;
class TimeService;
class WeatherService;
class WifiService;

struct AppContext {
    AlarmService &alarms;
    NotificationService &notifications;
    SettingsService &settings;
    TimeService &time;
    WeatherService &weather;
    WifiService &wifi;
    SystemEventBus &events;
};
