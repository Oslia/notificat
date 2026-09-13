#pragma once

#include <cstdint>

#include "core/SystemEvent.hpp"
#include "lvgl.h"

class WifiService;

class WifiSettingsView final {
public:
    using ExitCallback = void (*)(void *context);

    void init(WifiService &wifi);
    void show(lv_obj_t *parent, ExitCallback exit_callback, void *exit_context);
    void hide();
    void onSystemEvents(SystemEventMask events);

private:
    static void rescan_cb(lv_event_t *event);
    static void network_selected_cb(lv_event_t *event);
    static void connect_cb(lv_event_t *event);
    static void disconnect_cb(lv_event_t *event);
    static void cancel_password_cb(lv_event_t *event);
    static void password_input_event_cb(lv_event_t *event);
    static void keyboard_event_cb(lv_event_t *event);
    static void exit_cb(lv_event_t *event);

    void update();
    void show_networks();
    void show_password_form();
    void set_keyboard_visible(bool visible);

    lv_obj_t *root_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *network_list_ = nullptr;
    lv_obj_t *disconnect_button_ = nullptr;
    lv_obj_t *password_input_ = nullptr;
    lv_obj_t *keyboard_ = nullptr;
    WifiService *wifi_ = nullptr;
    uint32_t displayed_generation_ = UINT32_MAX;
    char selected_ssid_[33]{};
    ExitCallback exit_callback_ = nullptr;
    void *exit_context_ = nullptr;
};
