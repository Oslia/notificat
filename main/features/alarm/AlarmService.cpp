#include "features/alarm/AlarmService.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iterator>

#include "features/alarm/AlarmOutput.hpp"
#include "features/notification/NotificationService.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "platform/time/TimeService.hpp"

namespace {

constexpr char kAlarmNamespace[] = "alarms";
constexpr char kScheduleKey[] = "schedule";
constexpr uint32_t kStorageMagic = 0x414C4D31;
constexpr uint16_t kStorageVersion = 2;
constexpr uint16_t kLegacyStorageVersion = 1;
constexpr uint32_t kAlarmTaskStackSize = 4096;
constexpr TickType_t kAlarmCheckInterval = pdMS_TO_TICKS(1000);
constexpr int64_t kMaximumRingSeconds = 10 * 60;

struct StoredAlarm {
    uint32_t id;
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;
    uint8_t repeat_days;
};

/* NVS 上のバイナリ形式。既存データとの互換性なしに並びや型を変更しないこと。 */
struct StoredSchedule {
    uint32_t magic;
    uint16_t version;
    uint8_t count;
    uint8_t reserved;
    uint32_t next_id;
    StoredAlarm alarms[kMaximumAlarmCount];
};

static_assert(sizeof(StoredAlarm) == 8, "Alarm storage layout must remain compatible with v1");

bool valid_time(uint8_t hour, uint8_t minute)
{
    return hour < 24 && minute < 60;
}

bool valid_repeat_days(uint8_t repeat_days)
{
    return (repeat_days & static_cast<uint8_t>(~kAlarmEveryDay)) == 0;
}

} // namespace

AlarmService &AlarmService::instance()
{
    static AlarmService service;
    return service;
}

void AlarmService::init(TimeService &time, NotificationService &notifications,
                        AlarmOutput &output, SystemEventBus &events)
{
    if (initialized_) {
        return;
    }

    event_bus_ = &events;
    time_ = &time;
    notifications_ = &notifications;
    output_ = &output;
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        snapshot_.last_error = ESP_ERR_NO_MEM;
        publish_changed();
        return;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    snapshot_.last_error = load_locked();
    update_counts_locked();
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));

    TaskHandle_t task_handle = nullptr;
    if (xTaskCreate(task_entry, "alarm", kAlarmTaskStackSize,
                    this, 3, &task_handle) != pdPASS) {
        snapshot_.last_error = ESP_ERR_NO_MEM;
        publish_changed();
        return;
    }
    task_ = task_handle;
    initialized_ = true;
    publish_changed();
}

AlarmSnapshot AlarmService::snapshot() const
{
    if (mutex_ == nullptr) {
        return snapshot_;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const AlarmSnapshot result = snapshot_;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return result;
}

esp_err_t AlarmService::add(uint8_t hour, uint8_t minute, uint8_t repeat_days)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!valid_time(hour, minute) || !valid_repeat_days(repeat_days)) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    if (snapshot_.alarm_count >= kMaximumAlarmCount) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        return ESP_ERR_NO_MEM;
    }

    const uint32_t previous_next_id = next_id_;
    AlarmItem &alarm = snapshot_.alarms[snapshot_.alarm_count++];
    alarm.id = next_id_++;
    if (next_id_ == 0) {
        next_id_ = 1;
    }
    alarm.hour = hour;
    alarm.minute = minute;
    alarm.repeat_days = repeat_days;
    alarm.enabled = true;
    update_counts_locked();
    const esp_err_t result = save_locked();
    if (result != ESP_OK) {
        /* 永続化に失敗した場合、RAM 上の状態も保存前へ戻して食い違いを防ぐ。 */
        --snapshot_.alarm_count;
        snapshot_.alarms[snapshot_.alarm_count] = {};
        next_id_ = previous_next_id;
        update_counts_locked();
    }
    snapshot_.last_error = result;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();
    return result;
}

esp_err_t AlarmService::update(uint32_t id, uint8_t hour, uint8_t minute, uint8_t repeat_days)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!valid_time(hour, minute) || !valid_repeat_days(repeat_days)) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const int index = find_index_locked(id);
    if (index < 0) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        return ESP_ERR_NOT_FOUND;
    }
    const AlarmItem previous_alarm = snapshot_.alarms[index];
    const int64_t previous_trigger = last_trigger_minute_[index];
    snapshot_.alarms[index].hour = hour;
    snapshot_.alarms[index].minute = minute;
    snapshot_.alarms[index].repeat_days = repeat_days;
    last_trigger_minute_[index] = 0;
    const esp_err_t result = save_locked();
    if (result != ESP_OK) {
        /* NVS と公開スナップショットは常に同じ設定を表す。 */
        snapshot_.alarms[index] = previous_alarm;
        last_trigger_minute_[index] = previous_trigger;
    }
    snapshot_.last_error = result;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    publish_changed();
    return result;
}

esp_err_t AlarmService::remove(uint32_t id)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    bool stop_output = false;
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const int index = find_index_locked(id);
    if (index < 0) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        return ESP_ERR_NOT_FOUND;
    }

    const AlarmSnapshot previous_snapshot = snapshot_;
    int64_t previous_triggers[kMaximumAlarmCount];
    std::copy(std::begin(last_trigger_minute_), std::end(last_trigger_minute_),
              std::begin(previous_triggers));
    const int64_t previous_snooze_due = snooze_due_;
    const uint32_t previous_snooze_id = snooze_alarm_id_;
    const int64_t previous_ring_started = ring_started_at_;

    stop_output = snapshot_.ringing && snapshot_.active_alarm_id == id;
    for (uint8_t item = static_cast<uint8_t>(index); item + 1 < snapshot_.alarm_count; ++item) {
        snapshot_.alarms[item] = snapshot_.alarms[item + 1];
        last_trigger_minute_[item] = last_trigger_minute_[item + 1];
    }
    --snapshot_.alarm_count;
    snapshot_.alarms[snapshot_.alarm_count] = {};
    last_trigger_minute_[snapshot_.alarm_count] = 0;
    if (stop_output) {
        snapshot_.ringing = false;
        snapshot_.active_alarm_id = 0;
        ring_started_at_ = 0;
    }
    if (snooze_alarm_id_ == id) {
        snooze_alarm_id_ = 0;
        snooze_due_ = 0;
    }
    update_counts_locked();
    const esp_err_t result = save_locked();
    if (result != ESP_OK) {
        snapshot_ = previous_snapshot;
        std::copy(std::begin(previous_triggers), std::end(previous_triggers),
                  std::begin(last_trigger_minute_));
        snooze_due_ = previous_snooze_due;
        snooze_alarm_id_ = previous_snooze_id;
        ring_started_at_ = previous_ring_started;
        stop_output = false;
    }
    snapshot_.last_error = result;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));

    if (stop_output) {
        output_->set_active(false);
    }
    publish_changed();
    return result;
}

esp_err_t AlarmService::set_enabled(uint32_t id, bool enabled)
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    bool stop_output = false;
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const int index = find_index_locked(id);
    if (index < 0) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        return ESP_ERR_NOT_FOUND;
    }
    const AlarmItem previous_alarm = snapshot_.alarms[index];
    const int64_t previous_trigger = last_trigger_minute_[index];
    const bool previous_ringing = snapshot_.ringing;
    const uint32_t previous_active_id = snapshot_.active_alarm_id;
    const int64_t previous_ring_started = ring_started_at_;
    const int64_t previous_snooze_due = snooze_due_;
    const uint32_t previous_snooze_id = snooze_alarm_id_;

    snapshot_.alarms[index].enabled = enabled;
    last_trigger_minute_[index] = 0;
    if (!enabled && snapshot_.ringing && snapshot_.active_alarm_id == id) {
        snapshot_.ringing = false;
        snapshot_.active_alarm_id = 0;
        ring_started_at_ = 0;
        stop_output = true;
    }
    if (!enabled && snooze_alarm_id_ == id) {
        snooze_alarm_id_ = 0;
        snooze_due_ = 0;
    }
    update_counts_locked();
    const esp_err_t result = save_locked();
    if (result != ESP_OK) {
        snapshot_.alarms[index] = previous_alarm;
        last_trigger_minute_[index] = previous_trigger;
        snapshot_.ringing = previous_ringing;
        snapshot_.active_alarm_id = previous_active_id;
        ring_started_at_ = previous_ring_started;
        snooze_due_ = previous_snooze_due;
        snooze_alarm_id_ = previous_snooze_id;
        stop_output = false;
        update_counts_locked();
    }
    snapshot_.last_error = result;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));

    if (stop_output) {
        output_->set_active(false);
    }
    publish_changed();
    return result;
}

esp_err_t AlarmService::dismiss()
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    snapshot_.ringing = false;
    snapshot_.active_alarm_id = 0;
    snooze_due_ = 0;
    snooze_alarm_id_ = 0;
    ring_started_at_ = 0;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    output_->set_active(false);
    publish_changed();
    return ESP_OK;
}

esp_err_t AlarmService::snooze(uint16_t minutes)
{
    if (!initialized_ || minutes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    if (!snapshot_.ringing) {
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
        return ESP_ERR_INVALID_STATE;
    }
    snooze_due_ = static_cast<int64_t>(std::time(nullptr)) + static_cast<int64_t>(minutes) * 60;
    snooze_alarm_id_ = snapshot_.active_alarm_id;
    snapshot_.ringing = false;
    snapshot_.active_alarm_id = 0;
    ring_started_at_ = 0;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    output_->set_active(false);
    publish_changed();
    if (task_ != nullptr) {
        xTaskNotifyGive(static_cast<TaskHandle_t>(task_));
    }
    return ESP_OK;
}

void AlarmService::task_entry(void *context)
{
    static_cast<AlarmService *>(context)->run();
}

void AlarmService::run()
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, kAlarmCheckInterval);

        const SystemTime system_time = time_->local_time();
        if (!system_time.valid) {
            continue;
        }

        const int64_t now = static_cast<int64_t>(std::time(nullptr));
        /* 1 秒周期で確認しても、同じアラームを同一分内で複数回鳴らさない。 */
        const int64_t minute_key = now / 60;
        bool start_output = false;
        bool stop_output = false;
        bool state_changed = false;
        AlarmItem triggered_alarm{};

        xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
        if (snapshot_.ringing && ring_started_at_ > 0
            && now - ring_started_at_ >= kMaximumRingSeconds) {
            snapshot_.ringing = false;
            snapshot_.active_alarm_id = 0;
            ring_started_at_ = 0;
            stop_output = true;
            state_changed = true;
        }

        if (!snapshot_.ringing && snooze_due_ > 0 && now >= snooze_due_) {
            snapshot_.ringing = true;
            snapshot_.active_alarm_id = snooze_alarm_id_;
            snapshot_.active_hour = static_cast<uint8_t>(system_time.local.tm_hour);
            snapshot_.active_minute = static_cast<uint8_t>(system_time.local.tm_min);
            ring_started_at_ = now;
            snooze_due_ = 0;
            snooze_alarm_id_ = 0;
            start_output = true;
            state_changed = true;
            triggered_alarm.id = snapshot_.active_alarm_id;
            triggered_alarm.hour = snapshot_.active_hour;
            triggered_alarm.minute = snapshot_.active_minute;
        } else if (!snapshot_.ringing) {
            int first_match = -1;
            /* 同時刻の候補はすべて処理済みにし、音を鳴らすのは先頭の一件だけにする。 */
            for (uint8_t index = 0; index < snapshot_.alarm_count; ++index) {
                const AlarmItem &alarm = snapshot_.alarms[index];
                if (alarm.enabled && alarm.hour == system_time.local.tm_hour
                    && alarm.minute == system_time.local.tm_min
                    && (alarm.repeat_days == 0
                        || (alarm.repeat_days & (1U << system_time.local.tm_wday)) != 0)
                    && last_trigger_minute_[index] != minute_key) {
                    last_trigger_minute_[index] = minute_key;
                    if (first_match < 0) {
                        first_match = index;
                    }
                }
            }

            if (first_match >= 0) {
                triggered_alarm = snapshot_.alarms[first_match];
                if (triggered_alarm.repeat_days == 0) {
                    /* 曜日未指定は一回限りとし、鳴動開始前に無効化を保存する。 */
                    snapshot_.alarms[first_match].enabled = false;
                    update_counts_locked();
                    snapshot_.last_error = save_locked();
                }
                snapshot_.ringing = true;
                snapshot_.active_alarm_id = triggered_alarm.id;
                snapshot_.active_hour = triggered_alarm.hour;
                snapshot_.active_minute = triggered_alarm.minute;
                ring_started_at_ = now;
                start_output = true;
                state_changed = true;
            }
        }
        xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));

        /* 音声出力と通知は時間がかかり得るため、サービスの mutex 外で実行する。 */
        if (stop_output) {
            output_->set_active(false);
        }
        if (start_output) {
            output_->set_active(true);
            char message[48];
            std::snprintf(message, sizeof(message), "Alarm %02u:%02u",
                          triggered_alarm.hour, triggered_alarm.minute);
            notifications_->post(message);
        }
        if (state_changed) {
            publish_changed();
        }
    }
}

int AlarmService::find_index_locked(uint32_t id) const
{
    for (uint8_t index = 0; index < snapshot_.alarm_count; ++index) {
        if (snapshot_.alarms[index].id == id) {
            return index;
        }
    }
    return -1;
}

void AlarmService::update_counts_locked()
{
    snapshot_.enabled_count = 0;
    for (uint8_t index = 0; index < snapshot_.alarm_count; ++index) {
        if (snapshot_.alarms[index].enabled) {
            ++snapshot_.enabled_count;
        }
    }
}

esp_err_t AlarmService::load_locked()
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open(kAlarmNamespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }

    StoredSchedule stored{};
    size_t size = sizeof(stored);
    result = nvs_get_blob(handle, kScheduleKey, &stored, &size);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    if (size != sizeof(stored) || stored.magic != kStorageMagic
        || (stored.version != kStorageVersion && stored.version != kLegacyStorageVersion)
        || stored.count > kMaximumAlarmCount) {
        return ESP_ERR_INVALID_VERSION;
    }

    snapshot_.alarm_count = 0;
    uint32_t maximum_id = 0;
    for (uint8_t index = 0; index < stored.count; ++index) {
        const StoredAlarm &source = stored.alarms[index];
        if (source.id == 0 || !valid_time(source.hour, source.minute)) {
            continue;
        }
        AlarmItem &target = snapshot_.alarms[snapshot_.alarm_count++];
        target.id = source.id;
        target.hour = source.hour;
        target.minute = source.minute;
        /* v1 には曜日設定がないため、従来動作の毎日に移行する。 */
        target.repeat_days = stored.version == kLegacyStorageVersion
                                 ? kAlarmEveryDay
                                 : source.repeat_days;
        if (!valid_repeat_days(target.repeat_days)) {
            target.repeat_days = kAlarmEveryDay;
        }
        target.enabled = source.enabled != 0;
        maximum_id = std::max(maximum_id, source.id);
    }
    next_id_ = std::max(stored.next_id, maximum_id + 1);
    if (next_id_ == 0) {
        next_id_ = 1;
    }
    return stored.version == kLegacyStorageVersion ? save_locked() : ESP_OK;
}

esp_err_t AlarmService::save_locked()
{
    StoredSchedule stored{};
    stored.magic = kStorageMagic;
    stored.version = kStorageVersion;
    stored.count = snapshot_.alarm_count;
    stored.next_id = next_id_;
    for (uint8_t index = 0; index < snapshot_.alarm_count; ++index) {
        const AlarmItem &source = snapshot_.alarms[index];
        StoredAlarm &target = stored.alarms[index];
        target.id = source.id;
        target.hour = source.hour;
        target.minute = source.minute;
        target.enabled = source.enabled ? 1 : 0;
        target.repeat_days = source.repeat_days;
    }

    nvs_handle_t handle;
    esp_err_t result = nvs_open(kAlarmNamespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_set_blob(handle, kScheduleKey, &stored, sizeof(stored));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

void AlarmService::publish_changed()
{
    if (event_bus_ != nullptr) {
        event_bus_->publish(SystemEvent::AlarmChanged);
    }
}
