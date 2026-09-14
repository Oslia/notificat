#include "platform/time/TimeService.hpp"

#include <cstdlib>

#include "esp_netif_sntp.h"

namespace {

/* 未同期時の初期値を実時刻と誤認しないための妥当性判定境界。 */
constexpr std::time_t kEarliestValidTime = 1704067200; // 2024年1月1日 00:00:00 UTC

} // namespace

TimeService &TimeService::instance()
{
    static TimeService service;
    return service;
}

void TimeService::init(const char *timezone, SystemEventBus &events)
{
    event_bus_ = &events;
    apply_timezone(timezone);
    initialized_ = true;
}

void TimeService::apply_timezone(const char *timezone)
{
    setenv("TZ", timezone != nullptr ? timezone : "UTC0", 1);
    tzset();
    if (event_bus_ != nullptr) {
        event_bus_->publish(SystemEvent::TimeChanged);
    }
}

esp_err_t TimeService::start_time_sync()
{
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    const TimeSyncState state = sync_state_.load();
    if (state == TimeSyncState::Synchronizing || state == TimeSyncState::Synchronized) {
        return ESP_OK;
    }

    sync_state_.store(TimeSyncState::Synchronizing);
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    /* 接続コールバックをブロックせず、完了は time_sync_cb から通知する。 */
    config.wait_for_sync = false;
    config.sync_cb = &TimeService::time_sync_cb;
    const esp_err_t result = esp_netif_sntp_init(&config);
    if (result != ESP_OK) {
        sync_state_.store(TimeSyncState::Failed);
    }
    return result;
}

TimeSyncState TimeService::sync_state() const
{
    return sync_state_.load();
}

SystemTime TimeService::local_time() const
{
    SystemTime result{};
    const std::time_t now = std::time(nullptr);
    result.valid = now >= kEarliestValidTime;
    if (result.valid) {
        localtime_r(&now, &result.local);
    }
    return result;
}

void TimeService::time_sync_cb(struct timeval *time_value)
{
    (void)time_value;
    TimeService &service = TimeService::instance();
    service.sync_state_.store(TimeSyncState::Synchronized);
    if (service.event_bus_ != nullptr) {
        service.event_bus_->publish(SystemEvent::TimeChanged);
    }
}
