#include <cstdint>

#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "ui/AppShell.hpp"
#include "ui/UiTheme.hpp"
#include "core/SystemManager.hpp"
#include "core/SystemMQTT.hpp"
#include "features/alarm/AlarmService.hpp"
#include "platform/network/WifiService.hpp"
#include "ui/app/AppManager.hpp"

namespace {

AppManager s_app_manager;
AppContext *s_context = nullptr;
lv_obj_t *s_app_layer = nullptr;
lv_obj_t *s_app_selector = nullptr;
lv_obj_t *s_app_selector_sheet = nullptr;
lv_obj_t *s_wifi_status_label = nullptr;
lv_obj_t *s_system_status_label = nullptr;
lv_obj_t *s_alarm_overlay = nullptr;
lv_obj_t *s_alarm_time_label = nullptr;
int32_t s_selector_swipe_start_y = -1;
bool s_selector_dismiss_candidate = false;
int32_t s_launcher_swipe_start_y = -1;
bool s_launcher_swipe_candidate = false;

constexpr int32_t kStatusBarHeight = 24;
constexpr int32_t kAppSelectorHeight = 208;
constexpr uint32_t kAppSelectorShowAnimationMs = 150;
constexpr uint32_t kAppSelectorHideAnimationMs = 120;
constexpr int32_t kLauncherGestureEdgeHeight = 16;
constexpr int32_t kLauncherGestureDistance = 48;

void set_object_y(void *object, int32_t y)
{
    lv_obj_set_y(static_cast<lv_obj_t *>(object), y);
}

void delete_app_selector(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    if (s_app_selector != nullptr) {
        lv_obj_delete(s_app_selector);
    }
    s_app_selector = nullptr;
    s_app_selector_sheet = nullptr;
}

void hide_app_selector()
{
    if (s_app_selector_sheet == nullptr) {
        return;
    }

    /* 同じオブジェクトを操作中のアニメーションを止め、二重完了を防ぐ。 */
    lv_anim_delete(s_app_selector_sheet, set_object_y);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_app_selector_sheet);
    lv_anim_set_values(&animation, lv_obj_get_y(s_app_selector_sheet),
                       lv_display_get_vertical_resolution(lv_display_get_default()));
    lv_anim_set_duration(&animation, kAppSelectorHideAnimationMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&animation, set_object_y);
    lv_anim_set_completed_cb(&animation, delete_app_selector);
    lv_anim_start(&animation);
}

void activate_app(AppId id)
{
    if (s_app_selector != nullptr) {
        if (s_app_selector_sheet != nullptr) {
            lv_anim_delete(s_app_selector_sheet, set_object_y);
        }
        lv_obj_delete(s_app_selector);
        s_app_selector = nullptr;
        s_app_selector_sheet = nullptr;
    }

    s_app_manager.activate(id, s_app_layer);
}

void app_selector_event_cb(lv_event_t *event)
{
    /* AppId は小さな列挙値であり、コールバック用データとして所有権なしで渡す。 */
    const auto id = static_cast<AppId>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    activate_app(id);
}

void create_app_selector_button(lv_obj_t *parent, const char *text, AppId id)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_width(button, lv_pct(100));
    lv_obj_set_height(button, 48);
    lv_obj_set_style_pad_all(button, 7, 0);
    UiTheme::apply_button(button);
    lv_obj_add_event_cb(button, app_selector_event_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(id)));

    lv_obj_t *badge = lv_obj_create(button);
    lv_obj_set_size(badge, 32, 32);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, UiTheme::soft_accent(), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 0, 0);

    char initial[2] = {text[0], '\0'};
    lv_obj_t *initial_label = lv_label_create(badge);
    lv_label_set_text(initial_label, initial);
    lv_obj_set_style_text_color(initial_label, UiTheme::text(), 0);
    lv_obj_center(initial_label);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, UiTheme::text(), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 44, 0);
}

void show_app_selector();

void app_selector_touch_event_cb(lv_event_t *event)
{
    if (s_alarm_overlay != nullptr) {
        return;
    }

    lv_indev_t *indev = static_cast<lv_indev_t *>(lv_event_get_user_data(event));
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (s_app_selector == nullptr) {
        switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            if (point.y >= lv_display_get_vertical_resolution(lv_display_get_default()) - kLauncherGestureEdgeHeight) {
                s_launcher_swipe_start_y = point.y;
                s_launcher_swipe_candidate = true;
            }
            break;
        case LV_EVENT_RELEASED:
            if (s_launcher_swipe_candidate && s_launcher_swipe_start_y - point.y >= kLauncherGestureDistance) {
                show_app_selector();
            }
            s_launcher_swipe_candidate = false;
            s_launcher_swipe_start_y = -1;
            break;
        case LV_EVENT_PRESS_LOST:
            s_launcher_swipe_candidate = false;
            s_launcher_swipe_start_y = -1;
            break;
        default:
            break;
        }
        return;
    }

    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED: {
        /* リストを先頭まで戻した時だけ下スワイプを閉じる操作として扱う。 */
        if (s_app_selector_sheet == nullptr || lv_obj_get_scroll_top(s_app_selector_sheet) != 0) {
            break;
        }

        const int32_t sheet_top = lv_obj_get_y(s_app_selector_sheet);
        const int32_t sheet_bottom = sheet_top + lv_obj_get_height(s_app_selector_sheet);
        if (point.y >= sheet_top && point.y <= sheet_bottom) {
            s_selector_swipe_start_y = point.y;
            s_selector_dismiss_candidate = true;
        }
        break;
    }
    case LV_EVENT_RELEASED:
        if (s_selector_dismiss_candidate && point.y - s_selector_swipe_start_y >= kAppSelectorHeight / 4) {
            hide_app_selector();
        }
        s_selector_dismiss_candidate = false;
        s_selector_swipe_start_y = -1;
        break;
    case LV_EVENT_PRESS_LOST:
        s_selector_dismiss_candidate = false;
        s_selector_swipe_start_y = -1;
        break;
    default:
        break;
    }
}

void show_app_selector()
{
    if (s_app_selector != nullptr) {
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    s_app_selector = lv_obj_create(screen);
    lv_obj_set_size(s_app_selector, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_app_selector, UiTheme::scrim(), 0);
    lv_obj_set_style_bg_opa(s_app_selector, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_app_selector, 0, 0);
    lv_obj_set_style_pad_all(s_app_selector, 0, 0);
    lv_obj_clear_flag(s_app_selector, LV_OBJ_FLAG_SCROLLABLE);

    s_app_selector_sheet = lv_obj_create(s_app_selector);
    lv_obj_set_width(s_app_selector_sheet, lv_pct(100));
    lv_obj_set_height(s_app_selector_sheet, kAppSelectorHeight);
    lv_obj_set_pos(s_app_selector_sheet, 0,
                   lv_display_get_vertical_resolution(lv_display_get_default()));
    lv_obj_set_style_pad_all(s_app_selector_sheet, 14, 0);
    lv_obj_set_style_pad_gap(s_app_selector_sheet, 7, 0);
    UiTheme::apply_card(s_app_selector_sheet);
    lv_obj_set_style_radius(s_app_selector_sheet, 26, 0);
    lv_obj_set_flex_flow(s_app_selector_sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_app_selector_sheet, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    UiTheme::enable_scroll(s_app_selector_sheet, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_app_selector_sheet, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(s_app_selector_sheet, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *handle = lv_obj_create(s_app_selector_sheet);
    lv_obj_set_size(handle, 36, 4);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, UiTheme::soft_accent(), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_app_selector_sheet);
    lv_label_set_text(title, "NOTIFICAT  =^.^=");
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    lv_obj_t *hint = lv_label_create(s_app_selector_sheet);
    lv_label_set_text(hint, "Choose a cozy corner");
    UiTheme::apply_muted_text(hint);
    const AppRegistry &registry = s_app_manager.registry();
    for (size_t index = 0; index < registry.count(); ++index) {
        const AppDescriptor &descriptor = registry.at(index);
        create_app_selector_button(s_app_selector_sheet, descriptor.title, descriptor.id);
    }

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_app_selector_sheet);
    lv_anim_set_values(&animation, lv_display_get_vertical_resolution(lv_display_get_default()),
                       lv_display_get_vertical_resolution(lv_display_get_default()) - kAppSelectorHeight);
    lv_anim_set_duration(&animation, kAppSelectorShowAnimationMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, set_object_y);
    lv_anim_start(&animation);
}

void alarm_snooze_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_context != nullptr) {
        s_context->alarms.snooze(5);
    }
}

void alarm_dismiss_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_context != nullptr) {
        s_context->alarms.dismiss();
    }
}

void create_alarm_button(lv_obj_t *parent, const char *text, lv_align_t align,
                         int32_t x, lv_event_cb_t callback, bool primary)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 128, 48);
    lv_obj_align(button, align, x, -18);
    UiTheme::apply_button(button, primary);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

void update_alarm_overlay()
{
    if (s_context == nullptr) {
        return;
    }

    const AlarmSnapshot alarm = s_context->alarms.snapshot();
    if (!alarm.ringing) {
        if (s_alarm_overlay != nullptr) {
            lv_obj_delete(s_alarm_overlay);
            s_alarm_overlay = nullptr;
            s_alarm_time_label = nullptr;
        }
        return;
    }

    if (s_alarm_overlay == nullptr) {
        if (s_app_selector != nullptr) {
            if (s_app_selector_sheet != nullptr) {
                lv_anim_delete(s_app_selector_sheet, set_object_y);
            }
            lv_obj_delete(s_app_selector);
            s_app_selector = nullptr;
            s_app_selector_sheet = nullptr;
        }

        /* アラーム操作はステータスバーやランチャーより常に手前へ表示する。 */
        s_alarm_overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(s_alarm_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_pos(s_alarm_overlay, 0, 0);
        lv_obj_set_style_pad_all(s_alarm_overlay, 0, 0);
        UiTheme::apply_page(s_alarm_overlay);
        lv_obj_add_flag(s_alarm_overlay, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *title = lv_label_create(s_alarm_overlay);
        lv_label_set_text(title, "WAKE UP  =^.^=");
        lv_obj_set_style_text_color(title, UiTheme::accent(), 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

        s_alarm_time_label = lv_label_create(s_alarm_overlay);
        lv_obj_set_style_text_font(s_alarm_time_label, &lv_font_montserrat_42, 0);
        lv_obj_set_style_text_color(s_alarm_time_label, UiTheme::text(), 0);
        lv_obj_align(s_alarm_time_label, LV_ALIGN_CENTER, 0, -30);

        lv_obj_t *message = lv_label_create(s_alarm_overlay);
        lv_label_set_text(message, "Daily alarm is ringing");
        UiTheme::apply_muted_text(message);
        lv_obj_align(message, LV_ALIGN_CENTER, 0, 16);

        create_alarm_button(s_alarm_overlay, "Snooze 5m", LV_ALIGN_BOTTOM_LEFT,
                            18, alarm_snooze_cb, false);
        create_alarm_button(s_alarm_overlay, "Stop", LV_ALIGN_BOTTOM_RIGHT,
                            -18, alarm_dismiss_cb, true);
    }

    lv_label_set_text_fmt(s_alarm_time_label, "%02u:%02u",
                          alarm.active_hour, alarm.active_minute);
}

void update_alarm_indicator()
{
    if (s_context == nullptr || s_system_status_label == nullptr) {
        return;
    }
    const AlarmSnapshot alarm = s_context->alarms.snapshot();
    if (alarm.enabled_count == 0) {
        lv_label_set_text(s_system_status_label, "=^.^=");
    } else {
        lv_label_set_text_fmt(s_system_status_label, "=^.^= A%u", alarm.enabled_count);
    }
}

void status_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (s_context == nullptr) {
        return;
    }

    /* サービス側は LVGL を触らず、UI タイマー上でイベントを画面へ反映する。 */
    s_context->mqtt.dispatch();
    system_mqtt_test_poll(s_context->mqtt);
    const SystemEventMask events = s_context->events.consume();
    if (events == 0) {
        return;
    }

    s_app_manager.dispatchSystemEvents(events);
    if (has_system_event(events, SystemEvent::AlarmChanged)) {
        update_alarm_indicator();
        update_alarm_overlay();
    }
    if (!has_system_event(events, SystemEvent::WifiChanged)) {
        return;
    }

    WifiSnapshot snapshot{};
    s_context->wifi.snapshot(snapshot);

    if (snapshot.connection_state == WifiConnectionState::Connected) {
        lv_label_set_text_fmt(s_wifi_status_label, "Wi-Fi  %s", snapshot.connected_ssid);
    } else if (snapshot.connection_state == WifiConnectionState::Connecting) {
        lv_label_set_text(s_wifi_status_label, "Wi-Fi  Connecting...");
    } else if (snapshot.connection_state == WifiConnectionState::Disconnecting) {
        lv_label_set_text(s_wifi_status_label, "Wi-Fi  Disconnecting...");
    } else if (snapshot.connection_state == WifiConnectionState::Error) {
        lv_label_set_text(s_wifi_status_label, "Wi-Fi  Error");
    } else if (snapshot.scan_in_progress) {
        lv_label_set_text(s_wifi_status_label, "Wi-Fi  Scanning...");
    } else if (snapshot.connection_state == WifiConnectionState::Disconnected) {
        lv_label_set_text(s_wifi_status_label, "Wi-Fi  Disconnected");
    } else {
        lv_label_set_text(s_wifi_status_label, "Wi-Fi  Off");
    }
}

void create_status_bar()
{
    lv_obj_t *bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(bar, lv_pct(100), kStatusBarHeight);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, UiTheme::surface(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, UiTheme::border(), 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 8, 0);
    lv_obj_set_style_pad_ver(bar, 2, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_wifi_status_label = lv_label_create(bar);
    lv_label_set_long_mode(s_wifi_status_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(s_wifi_status_label, 205);
    lv_obj_align(s_wifi_status_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(s_wifi_status_label, "Wi-Fi  Off");
    UiTheme::apply_muted_text(s_wifi_status_label);

    s_system_status_label = lv_label_create(bar);
    lv_label_set_text(s_system_status_label, "=^.^=");
    lv_obj_set_style_text_color(s_system_status_label, UiTheme::accent(), 0);
    lv_obj_align(s_system_status_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_timer_create(status_timer_cb, 100, nullptr);
}

} // namespace

extern "C" void app_shell_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    UiTheme::init(lv_display_get_default());
    lv_obj_clean(screen);
    lv_obj_set_style_pad_all(screen, 0, 0);

    s_app_layer = lv_obj_create(screen);
    lv_obj_set_size(s_app_layer, lv_pct(100), lv_display_get_vertical_resolution(lv_display_get_default()) - kStatusBarHeight);
    lv_obj_set_pos(s_app_layer, 0, kStatusBarHeight);
    lv_obj_set_style_pad_all(s_app_layer, 0, 0);
    lv_obj_set_style_border_width(s_app_layer, 0, 0);
    UiTheme::apply_page(s_app_layer);

    lv_indev_t *touch_input = bsp_display_get_input_dev();
    lv_indev_add_event_cb(touch_input, app_selector_touch_event_cb, LV_EVENT_ALL, touch_input);

    SystemManager &system = SystemManager::instance();
    system.init();
    s_context = &system.context();
    create_status_bar();
    s_app_manager.init(*s_context);
    s_app_manager.activate(AppId::Home, s_app_layer);
}
