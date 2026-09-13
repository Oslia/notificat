#pragma once

#include "ui/app/App.hpp"

class AlarmApp final : public App {
public:
    void init(AppContext &context) override;
    void onEnter(lv_obj_t *parent) override;
    void onLeave() override;
    void onSystemEvents(SystemEventMask events) override;

private:
    void show_list();
    void show_editor(uint32_t alarm_id);
    void show_error(const char *message);
    uint8_t selected_repeat_days() const;

    static void add_cb(lv_event_t *event);
    static void edit_cb(lv_event_t *event);
    static void enabled_cb(lv_event_t *event);
    static void cancel_cb(lv_event_t *event);
    static void save_cb(lv_event_t *event);
    static void delete_cb(lv_event_t *event);

    AppContext *context_ = nullptr;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *hour_roller_ = nullptr;
    lv_obj_t *minute_roller_ = nullptr;
    lv_obj_t *day_buttons_[7]{};
    lv_obj_t *error_label_ = nullptr;
    uint32_t editing_alarm_id_ = 0;
    bool editor_visible_ = false;
};
