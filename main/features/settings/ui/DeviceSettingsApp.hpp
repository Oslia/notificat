#pragma once

#include "features/settings/ui/WifiSettingsView.hpp"
#include "ui/app/App.hpp"

class DeviceSettingsApp final : public App {
public:
    void init(AppContext &context) override;
    void onEnter(lv_obj_t *parent) override;
    void onLeave() override;
    void onSystemEvents(SystemEventMask events) override;

private:
    static void region_changed_cb(lv_event_t *event);
    static void network_settings_cb(lv_event_t *event);
    static void network_settings_exit_cb(void *context);

    void show_main();
    void show_network_settings();
    void update_network_state();

    lv_obj_t *root_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *network_state_label_ = nullptr;
    AppContext *context_ = nullptr;
    bool network_settings_visible_ = false;
    WifiSettingsView wifi_settings_view_;
};
