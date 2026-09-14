#include "features/notification/NotificationService.hpp"

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

NotificationService &NotificationService::instance()
{
    static NotificationService service;
    return service;
}

void NotificationService::init(SystemEventBus &events)
{
    event_bus_ = &events;
    if (mutex_ == nullptr) {
        mutex_ = xSemaphoreCreateMutex();
    }
    snapshot_ = {};
    event_bus_->publish(SystemEvent::NotificationChanged);
}

NotificationSnapshot NotificationService::snapshot() const
{
    if (mutex_ == nullptr) {
        return snapshot_;
    }
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const NotificationSnapshot result = snapshot_;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return result;
}

void NotificationService::post(const char *message)
{
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    /* 長時間未読でも uint8_t を周回させず、最大値で飽和させる。 */
    if (snapshot_.unread_count < UINT8_MAX) {
        ++snapshot_.unread_count;
    }
    std::strncpy(snapshot_.latest_message, message != nullptr ? message : "",
                 sizeof(snapshot_.latest_message) - 1);
    snapshot_.latest_message[sizeof(snapshot_.latest_message) - 1] = '\0';
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    if (event_bus_ != nullptr) {
        event_bus_->publish(SystemEvent::NotificationChanged);
    }
}

void NotificationService::mark_all_read()
{
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    snapshot_.unread_count = 0;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    if (event_bus_ != nullptr) {
        event_bus_->publish(SystemEvent::NotificationChanged);
    }
}
