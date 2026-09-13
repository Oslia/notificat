#pragma once

#include <cstdint>

#include "core/SystemEvent.hpp"
#include "esp_err.h"

class AlarmOutput;
class NotificationService;
class TimeService;

constexpr uint8_t kMaximumAlarmCount = 8;
constexpr uint8_t kAlarmDaySunday = 1U << 0;
constexpr uint8_t kAlarmDayMonday = 1U << 1;
constexpr uint8_t kAlarmDayTuesday = 1U << 2;
constexpr uint8_t kAlarmDayWednesday = 1U << 3;
constexpr uint8_t kAlarmDayThursday = 1U << 4;
constexpr uint8_t kAlarmDayFriday = 1U << 5;
constexpr uint8_t kAlarmDaySaturday = 1U << 6;
constexpr uint8_t kAlarmEveryDay = 0x7F;
constexpr uint8_t kAlarmWeekdays = kAlarmDayMonday | kAlarmDayTuesday
                                   | kAlarmDayWednesday | kAlarmDayThursday
                                   | kAlarmDayFriday;
constexpr uint8_t kAlarmWeekends = kAlarmDaySunday | kAlarmDaySaturday;

struct AlarmItem {
    uint32_t id = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t repeat_days = kAlarmEveryDay;
    bool enabled = true;
};

struct AlarmSnapshot {
    uint8_t alarm_count = 0;
    uint8_t enabled_count = 0;
    bool ringing = false;
    uint32_t active_alarm_id = 0;
    uint8_t active_hour = 0;
    uint8_t active_minute = 0;
    esp_err_t last_error = ESP_OK;
    AlarmItem alarms[kMaximumAlarmCount]{};
};

class AlarmService {
public:
    static AlarmService &instance();

    void init(TimeService &time, NotificationService &notifications,
              AlarmOutput &output, SystemEventBus &events);
    AlarmSnapshot snapshot() const;
    esp_err_t add(uint8_t hour, uint8_t minute, uint8_t repeat_days = kAlarmEveryDay);
    esp_err_t update(uint32_t id, uint8_t hour, uint8_t minute,
                     uint8_t repeat_days = kAlarmEveryDay);
    esp_err_t remove(uint32_t id);
    esp_err_t set_enabled(uint32_t id, bool enabled);
    esp_err_t dismiss();
    esp_err_t snooze(uint16_t minutes = 5);

private:
    AlarmService() = default;

    static void task_entry(void *context);
    void run();
    int find_index_locked(uint32_t id) const;
    void update_counts_locked();
    esp_err_t load_locked();
    esp_err_t save_locked();
    void publish_changed();

    SystemEventBus *event_bus_ = nullptr;
    TimeService *time_ = nullptr;
    NotificationService *notifications_ = nullptr;
    AlarmOutput *output_ = nullptr;
    void *mutex_ = nullptr;
    void *task_ = nullptr;
    bool initialized_ = false;
    uint32_t next_id_ = 1;
    int64_t last_trigger_minute_[kMaximumAlarmCount]{};
    int64_t snooze_due_ = 0;
    uint32_t snooze_alarm_id_ = 0;
    int64_t ring_started_at_ = 0;
    AlarmSnapshot snapshot_{};
};
