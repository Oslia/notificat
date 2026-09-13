#include "features/alarm/ui/AlarmApp.hpp"

#include <cstdio>
#include <cstdint>

#include "core/AppContext.hpp"
#include "features/alarm/AlarmService.hpp"
#include "platform/time/TimeService.hpp"
#include "ui/UiTheme.hpp"

namespace {

constexpr char kHourOptions[] =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23";
constexpr char kMinuteOptions[] =
    "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19"
    "\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39"
    "\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";
constexpr const char *kDayLabels[] = {"S", "M", "T", "W", "T", "F", "S"};
constexpr const char *kDayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

uint32_t object_alarm_id(lv_obj_t *object)
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(object)));
}

lv_obj_t *create_action_button(lv_obj_t *parent, const char *text, int32_t width,
                               lv_event_cb_t callback, void *context, bool primary = false)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, 42);
    UiTheme::apply_button(button, primary);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, context);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

void format_repeat_days(uint8_t repeat_days, char *buffer, size_t size)
{
    if (repeat_days == 0) {
        std::snprintf(buffer, size, "Once");
        return;
    }
    if (repeat_days == kAlarmEveryDay) {
        std::snprintf(buffer, size, "Every day");
        return;
    }
    if (repeat_days == kAlarmWeekdays) {
        std::snprintf(buffer, size, "Weekdays");
        return;
    }
    if (repeat_days == kAlarmWeekends) {
        std::snprintf(buffer, size, "Weekends");
        return;
    }

    size_t used = 0;
    buffer[0] = '\0';
    for (uint8_t day = 0; day < 7; ++day) {
        if ((repeat_days & (1U << day)) == 0) {
            continue;
        }
        const int written = std::snprintf(buffer + used, size - used, "%s%s",
                                          used == 0 ? "" : " ", kDayNames[day]);
        if (written < 0 || static_cast<size_t>(written) >= size - used) {
            break;
        }
        used += static_cast<size_t>(written);
    }
}

} // namespace

void AlarmApp::init(AppContext &context)
{
    context_ = &context;
}

void AlarmApp::onEnter(lv_obj_t *parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(root_, 0, 0);
    UiTheme::apply_page(root_);
    show_list();
}

void AlarmApp::onLeave()
{
    root_ = nullptr;
    hour_roller_ = nullptr;
    minute_roller_ = nullptr;
    for (lv_obj_t *&button : day_buttons_) {
        button = nullptr;
    }
    error_label_ = nullptr;
    editing_alarm_id_ = 0;
    editor_visible_ = false;
}

void AlarmApp::onSystemEvents(SystemEventMask events)
{
    if (root_ != nullptr && !editor_visible_
        && (has_system_event(events, SystemEvent::AlarmChanged)
            || has_system_event(events, SystemEvent::TimeChanged))) {
        show_list();
    }
}

void AlarmApp::show_list()
{
    if (root_ == nullptr) {
        return;
    }

    editor_visible_ = false;
    editing_alarm_id_ = 0;
    hour_roller_ = nullptr;
    minute_roller_ = nullptr;
    for (lv_obj_t *&button : day_buttons_) {
        button = nullptr;
    }
    error_label_ = nullptr;
    lv_obj_clean(root_);

    const AlarmSnapshot snapshot = context_->alarms.snapshot();

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text_fmt(title, "ALARMS  =^.^=  %u on", snapshot.enabled_count);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    lv_obj_t *add_button = create_action_button(root_, "+ Add", 72, add_cb, this, true);
    lv_obj_set_height(add_button, 34);
    lv_obj_align(add_button, LV_ALIGN_TOP_RIGHT, -12, 4);
    if (snapshot.alarm_count >= kMaximumAlarmCount) {
        lv_obj_add_state(add_button, LV_STATE_DISABLED);
    }

    lv_obj_t *status = lv_label_create(root_);
    const SystemTime system_time = context_->time.local_time();
    if (snapshot.last_error != ESP_OK) {
        lv_label_set_text(status, "Alarm storage error");
        lv_obj_set_style_text_color(status, UiTheme::accent(), 0);
    } else if (!system_time.valid) {
        lv_label_set_text(status, "Connect Wi-Fi to synchronize time");
        lv_obj_set_style_text_color(status, UiTheme::accent(), 0);
    } else {
        lv_label_set_text(status, "Tap an alarm to edit its schedule");
        UiTheme::apply_muted_text(status);
    }
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 39);

    lv_obj_t *list = lv_obj_create(root_);
    lv_obj_set_size(list, lv_pct(94), 154);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_pad_gap(list, 5, 0);
    UiTheme::apply_card(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    UiTheme::enable_scroll(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    if (snapshot.alarm_count == 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "No alarms\nTap + Add to create one");
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        UiTheme::apply_muted_text(empty);
        lv_obj_center(empty);
        return;
    }

    for (uint8_t index = 0; index < snapshot.alarm_count; ++index) {
        const AlarmItem &alarm = snapshot.alarms[index];
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), 54);
        lv_obj_set_style_pad_all(row, 7, 0);
        lv_obj_set_style_bg_color(row, UiTheme::surface_tint(), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, UiTheme::border(), 0);
        lv_obj_set_style_radius(row, 14, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row, reinterpret_cast<void *>(static_cast<uintptr_t>(alarm.id)));
        lv_obj_add_event_cb(row, edit_cb, LV_EVENT_CLICKED, this);

        lv_obj_t *time = lv_label_create(row);
        lv_label_set_text_fmt(time, "%02u:%02u", alarm.hour, alarm.minute);
        lv_obj_set_style_text_color(time, alarm.enabled ? UiTheme::text() : UiTheme::muted_text(), 0);
        lv_obj_align(time, LV_ALIGN_LEFT_MID, 3, -8);

        lv_obj_t *repeat = lv_label_create(row);
        char repeat_text[48];
        format_repeat_days(alarm.repeat_days, repeat_text, sizeof(repeat_text));
        lv_label_set_text(repeat, repeat_text);
        lv_obj_set_width(repeat, 190);
        lv_label_set_long_mode(repeat, LV_LABEL_LONG_MODE_DOTS);
        UiTheme::apply_muted_text(repeat);
        lv_obj_align(repeat, LV_ALIGN_LEFT_MID, 3, 11);

        lv_obj_t *toggle = lv_switch_create(row);
        lv_obj_set_size(toggle, 48, 26);
        lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -2, 0);
        if (alarm.enabled) {
            lv_obj_add_state(toggle, LV_STATE_CHECKED);
        }
        lv_obj_set_user_data(toggle, reinterpret_cast<void *>(static_cast<uintptr_t>(alarm.id)));
        lv_obj_add_event_cb(toggle, enabled_cb, LV_EVENT_VALUE_CHANGED, this);
    }
}

void AlarmApp::show_editor(uint32_t alarm_id)
{
    if (root_ == nullptr) {
        return;
    }

    uint8_t hour = 7;
    uint8_t minute = 0;
    uint8_t repeat_days = kAlarmEveryDay;
    if (alarm_id == 0) {
        const SystemTime current = context_->time.local_time();
        if (current.valid) {
            hour = static_cast<uint8_t>(current.local.tm_hour);
            const int suggested_minute = current.local.tm_min + 5;
            hour = static_cast<uint8_t>((hour + suggested_minute / 60) % 24);
            minute = static_cast<uint8_t>(suggested_minute % 60);
        }
    } else {
        const AlarmSnapshot snapshot = context_->alarms.snapshot();
        bool found = false;
        for (uint8_t index = 0; index < snapshot.alarm_count; ++index) {
            if (snapshot.alarms[index].id == alarm_id) {
                hour = snapshot.alarms[index].hour;
                minute = snapshot.alarms[index].minute;
                repeat_days = snapshot.alarms[index].repeat_days;
                found = true;
                break;
            }
        }
        if (!found) {
            show_list();
            return;
        }
    }

    editor_visible_ = true;
    editing_alarm_id_ = alarm_id;
    lv_obj_clean(root_);

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text(title, alarm_id == 0 ? "New alarm" : "Edit alarm");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    hour_roller_ = lv_roller_create(root_);
    lv_roller_set_options(hour_roller_, kHourOptions, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(hour_roller_, 3);
    lv_roller_set_selected(hour_roller_, hour, LV_ANIM_OFF);
    lv_obj_set_size(hour_roller_, 82, 84);
    lv_obj_align(hour_roller_, LV_ALIGN_TOP_MID, -52, 25);
    UiTheme::apply_card(hour_roller_);
    lv_obj_set_ext_click_area(hour_roller_, 6);
    lv_obj_set_style_text_color(hour_roller_, UiTheme::text(), 0);
    lv_obj_set_style_bg_color(hour_roller_, UiTheme::accent(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(hour_roller_, UiTheme::background(), LV_PART_SELECTED);

    lv_obj_t *separator = lv_label_create(root_);
    lv_label_set_text(separator, ":");
    lv_obj_set_style_text_color(separator, UiTheme::text(), 0);
    lv_obj_align(separator, LV_ALIGN_TOP_MID, 0, 57);

    minute_roller_ = lv_roller_create(root_);
    lv_roller_set_options(minute_roller_, kMinuteOptions, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(minute_roller_, 3);
    lv_roller_set_selected(minute_roller_, minute, LV_ANIM_OFF);
    lv_obj_set_size(minute_roller_, 82, 84);
    lv_obj_align(minute_roller_, LV_ALIGN_TOP_MID, 52, 25);
    UiTheme::apply_card(minute_roller_);
    lv_obj_set_ext_click_area(minute_roller_, 6);
    lv_obj_set_style_text_color(minute_roller_, UiTheme::text(), 0);
    lv_obj_set_style_bg_color(minute_roller_, UiTheme::accent(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(minute_roller_, UiTheme::background(), LV_PART_SELECTED);

    lv_obj_t *repeat_title = lv_label_create(root_);
    lv_label_set_text(repeat_title, "Repeat (none = once)");
    UiTheme::apply_muted_text(repeat_title);
    lv_obj_align(repeat_title, LV_ALIGN_TOP_LEFT, 22, 111);

    for (uint8_t day = 0; day < 7; ++day) {
        lv_obj_t *button = lv_button_create(root_);
        day_buttons_[day] = button;
        lv_obj_set_size(button, 36, 30);
        lv_obj_set_pos(button, 22 + day * 40, 129);
        UiTheme::apply_button(button);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_style_bg_color(button, UiTheme::accent(), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(button, UiTheme::surface(), LV_STATE_CHECKED);
        if ((repeat_days & (1U << day)) != 0) {
            lv_obj_add_state(button, LV_STATE_CHECKED);
        }

        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, kDayLabels[day]);
        lv_obj_center(label);
    }

    error_label_ = lv_label_create(root_);
    lv_label_set_text(error_label_, "");
    lv_obj_set_style_text_color(error_label_, UiTheme::accent(), 0);
    lv_obj_align(error_label_, LV_ALIGN_TOP_RIGHT, -22, 111);

    lv_obj_t *cancel = create_action_button(root_, "Cancel", alarm_id == 0 ? 126 : 86,
                                             cancel_cb, this);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, alarm_id == 0 ? 25 : 13, -7);

    lv_obj_t *save = create_action_button(root_, "Save", alarm_id == 0 ? 126 : 86,
                                           save_cb, this, true);
    lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, alarm_id == 0 ? -25 : -13, -7);

    if (alarm_id != 0) {
        lv_obj_t *remove = create_action_button(root_, "Delete", 86, delete_cb, this);
        lv_obj_set_style_bg_color(remove, UiTheme::danger(), 0);
        lv_obj_set_style_text_color(remove, UiTheme::surface(), 0);
        lv_obj_align(remove, LV_ALIGN_BOTTOM_MID, 0, -7);
    }
}

void AlarmApp::show_error(const char *message)
{
    if (error_label_ != nullptr) {
        lv_label_set_text(error_label_, message);
    }
}

uint8_t AlarmApp::selected_repeat_days() const
{
    uint8_t repeat_days = 0;
    for (uint8_t day = 0; day < 7; ++day) {
        if (day_buttons_[day] != nullptr
            && lv_obj_has_state(day_buttons_[day], LV_STATE_CHECKED)) {
            repeat_days |= static_cast<uint8_t>(1U << day);
        }
    }
    return repeat_days;
}

void AlarmApp::add_cb(lv_event_t *event)
{
    static_cast<AlarmApp *>(lv_event_get_user_data(event))->show_editor(0);
}

void AlarmApp::edit_cb(lv_event_t *event)
{
    auto *app = static_cast<AlarmApp *>(lv_event_get_user_data(event));
    app->show_editor(object_alarm_id(static_cast<lv_obj_t *>(lv_event_get_target(event))));
}

void AlarmApp::enabled_cb(lv_event_t *event)
{
    auto *app = static_cast<AlarmApp *>(lv_event_get_user_data(event));
    lv_obj_t *toggle = static_cast<lv_obj_t *>(lv_event_get_target(event));
    app->context_->alarms.set_enabled(object_alarm_id(toggle),
                                      lv_obj_has_state(toggle, LV_STATE_CHECKED));
}

void AlarmApp::cancel_cb(lv_event_t *event)
{
    static_cast<AlarmApp *>(lv_event_get_user_data(event))->show_list();
}

void AlarmApp::save_cb(lv_event_t *event)
{
    auto *app = static_cast<AlarmApp *>(lv_event_get_user_data(event));
    const uint8_t hour = static_cast<uint8_t>(lv_roller_get_selected(app->hour_roller_));
    const uint8_t minute = static_cast<uint8_t>(lv_roller_get_selected(app->minute_roller_));
    const uint8_t repeat_days = app->selected_repeat_days();
    const esp_err_t result = app->editing_alarm_id_ == 0
                                 ? app->context_->alarms.add(hour, minute, repeat_days)
                                 : app->context_->alarms.update(app->editing_alarm_id_, hour, minute,
                                                                repeat_days);
    if (result == ESP_OK) {
        app->show_list();
    } else {
        app->show_error("Unable to save alarm");
    }
}

void AlarmApp::delete_cb(lv_event_t *event)
{
    auto *app = static_cast<AlarmApp *>(lv_event_get_user_data(event));
    if (app->context_->alarms.remove(app->editing_alarm_id_) == ESP_OK) {
        app->show_list();
    } else {
        app->show_error("Unable to delete alarm");
    }
}
