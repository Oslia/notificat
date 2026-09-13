#pragma once

#include <cstdint>

#include "core/SystemEvent.hpp"

struct NotificationSnapshot {
    uint8_t unread_count = 0;
    char latest_message[96]{};
};

class NotificationService {
public:
    static NotificationService &instance();

    void init(SystemEventBus &events);
    NotificationSnapshot snapshot() const;
    void post(const char *message);
    void mark_all_read();

private:
    NotificationService() = default;

    SystemEventBus *event_bus_ = nullptr;
    void *mutex_ = nullptr;
    NotificationSnapshot snapshot_{};
};
