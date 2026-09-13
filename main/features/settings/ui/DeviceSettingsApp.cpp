#include "features/settings/ui/DeviceSettingsApp.hpp"

#include <cstdio>

#include "core/AppContext.hpp"
#include "features/weather/WeatherService.hpp"
#include "platform/network/WifiService.hpp"
#include "platform/settings/SettingsService.hpp"
#include "platform/time/TimeService.hpp"
#include "ui/UiTheme.hpp"

void DeviceSettingsApp::init(AppContext &context)
{
    context_ = &context;
    wifi_settings_view_.init(context.wifi);
}

void DeviceSettingsApp::onEnter(lv_obj_t *parent)
{
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(root_, 0, 0);
    UiTheme::apply_page(root_);
    show_main();
}

void DeviceSettingsApp::onLeave()
{
    wifi_settings_view_.hide();
    network_settings_visible_ = false;
    root_ = nullptr;
    status_label_ = nullptr;
    network_state_label_ = nullptr;
}

void DeviceSettingsApp::show_main()
{
    lv_obj_clean(root_);

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text(title, "SETTINGS  =^.^=");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    lv_obj_t *region_title = lv_label_create(root_);
    lv_label_set_text(region_title, "Region");
    lv_obj_align(region_title, LV_ALIGN_TOP_LEFT, 18, 42);
    UiTheme::apply_muted_text(region_title);

    network_settings_visible_ = false;
    SettingsService &settings = context_->settings;
    lv_obj_t *region_selector = lv_dropdown_create(root_);
    lv_obj_set_width(region_selector, lv_pct(88));
    lv_obj_align(region_selector, LV_ALIGN_TOP_MID, 0, 62);
    UiTheme::apply_card(region_selector);
    lv_obj_set_style_text_color(region_selector, UiTheme::text(), 0);
    char region_options[96]{};
    size_t used = 0;
    for (uint8_t index = 0; index < settings.region_count(); ++index) {
        const int written = std::snprintf(region_options + used, sizeof(region_options) - used,
                                          "%s%s", index == 0 ? "" : "\n", settings.region(index).name);
        if (written < 0 || static_cast<size_t>(written) >= sizeof(region_options) - used) {
            break;
        }
        used += static_cast<size_t>(written);
    }
    lv_dropdown_set_options(region_selector, region_options);
    lv_dropdown_set_selected(region_selector, settings.selected_region());
    lv_obj_add_event_cb(region_selector, region_changed_cb, LV_EVENT_VALUE_CHANGED, this);

    status_label_ = lv_label_create(root_);
    lv_label_set_text_fmt(status_label_, "%s: time and weather", settings.region().name);
    lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 18, 104);
    lv_obj_set_style_text_color(status_label_, UiTheme::accent(), 0);

    lv_obj_t *network_button = lv_button_create(root_);
    lv_obj_set_size(network_button, lv_pct(88), 48);
    lv_obj_align(network_button, LV_ALIGN_TOP_MID, 0, 136);
    UiTheme::apply_button(network_button, true);
    lv_obj_add_event_cb(network_button, network_settings_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *network_title = lv_label_create(network_button);
    lv_label_set_text(network_title, "Wi-Fi settings");
    lv_obj_align(network_title, LV_ALIGN_LEFT_MID, 8, -8);
    network_state_label_ = lv_label_create(network_button);
    lv_obj_align(network_state_label_, LV_ALIGN_LEFT_MID, 8, 10);
    lv_obj_set_style_text_color(network_state_label_, UiTheme::surface(), 0);
    lv_obj_set_style_text_opa(network_state_label_, LV_OPA_COVER, 0);
    update_network_state();
}

void DeviceSettingsApp::show_network_settings()
{
    lv_obj_clean(root_);
    status_label_ = nullptr;
    network_state_label_ = nullptr;
    network_settings_visible_ = true;
    wifi_settings_view_.show(root_, network_settings_exit_cb, this);
}

void DeviceSettingsApp::region_changed_cb(lv_event_t *event)
{
    auto *app = static_cast<DeviceSettingsApp *>(lv_event_get_user_data(event));
    lv_obj_t *selector = static_cast<lv_obj_t *>(lv_event_get_target(event));
    SettingsService &settings = app->context_->settings;
    const esp_err_t result = settings.set_region(lv_dropdown_get_selected(selector));
    if (result == ESP_OK) {
        app->context_->time.apply_timezone(settings.region().timezone);
        app->context_->weather.set_region(settings.region());
        lv_label_set_text_fmt(app->status_label_, "%s: time and weather", settings.region().name);
    } else {
        lv_label_set_text(app->status_label_, "Unable to save region");
    }
}

void DeviceSettingsApp::network_settings_cb(lv_event_t *event)
{
    static_cast<DeviceSettingsApp *>(lv_event_get_user_data(event))->show_network_settings();
}

void DeviceSettingsApp::network_settings_exit_cb(void *context)
{
    static_cast<DeviceSettingsApp *>(context)->show_main();
}

void DeviceSettingsApp::onSystemEvents(SystemEventMask events)
{
    if (network_settings_visible_) {
        wifi_settings_view_.onSystemEvents(events);
    } else if (has_system_event(events, SystemEvent::WifiChanged)) {
        update_network_state();
    }
}

void DeviceSettingsApp::update_network_state()
{
    if (network_state_label_ == nullptr) {
        return;
    }

    WifiSnapshot wifi{};
    context_->wifi.snapshot(wifi);
    lv_label_set_text(network_state_label_,
                      wifi.connection_state == WifiConnectionState::Connected
                          ? wifi.connected_ssid
                          : "Not connected");
}
