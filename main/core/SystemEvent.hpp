#pragma once

#include <atomic>
#include <cstdint>

enum class SystemEvent : uint32_t {
    None = 0,
    WifiChanged = 1U << 0,
    TimeChanged = 1U << 1,
    RegionChanged = 1U << 2,
    WeatherChanged = 1U << 3,
    AlarmChanged = 1U << 4,
    NotificationChanged = 1U << 5,
};

using SystemEventMask = uint32_t;

constexpr SystemEventMask system_event_mask(SystemEvent event)
{
    return static_cast<SystemEventMask>(event);
}

constexpr bool has_system_event(SystemEventMask events, SystemEvent event)
{
    return (events & system_event_mask(event)) != 0;
}

class SystemEventBus {
public:
    void publish(SystemEvent event)
    {
        /* 同種イベントは集約する。UI が必要とするのは回数ではなく最新状態である。 */
        pending_.fetch_or(system_event_mask(event), std::memory_order_release);
    }

    SystemEventMask consume()
    {
        /* 取得とクリアを一操作にし、別タスクからの publish を取りこぼさない。 */
        return pending_.exchange(0, std::memory_order_acq_rel);
    }

private:
    std::atomic<SystemEventMask> pending_{0};
};
