#pragma once

#include <atomic>
#include <cstdint>
#include <ctime>
#include <sys/time.h>

#include "core/SystemEvent.hpp"
#include "esp_err.h"

struct SystemTime {
    bool valid;
    std::tm local{};
};

enum class TimeSyncState : uint8_t {
    NotStarted,
    Synchronizing,
    Synchronized,
    Failed,
};

class TimeService {
public:
    static TimeService &instance();

    void init(const char *timezone, SystemEventBus &events);
    void apply_timezone(const char *timezone);
    esp_err_t start_time_sync();
    TimeSyncState sync_state() const;
    SystemTime local_time() const;

private:
    TimeService() = default;
    static void time_sync_cb(struct timeval *time_value);

    SystemEventBus *event_bus_ = nullptr;
    bool initialized_ = false;
    std::atomic<TimeSyncState> sync_state_{TimeSyncState::NotStarted};
};
