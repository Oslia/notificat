#include "features/settings/ui/WifiSettingsView.hpp"

#include <cstring>

#include "core/SystemEvent.hpp"
#include "platform/network/WifiService.hpp"
#include "ui/UiTheme.hpp"

namespace {

void set_text(char *destination, size_t size, const char *source)
{
    std::strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

const char *wifi_status_text(const WifiSnapshot &snapshot)
{
    switch (snapshot.last_error) {
    case WifiError::ScanStartFailed:
        return "Unable to start scan";
    case WifiError::ConnectStartFailed:
        return "Unable to start connection";
    case WifiError::ConnectionLost:
        return "Connection failed or disconnected";
    case WifiError::DisconnectFailed:
        return "Unable to disconnect";
    case WifiError::None:
        break;
    }

    if (snapshot.scan_in_progress) {
        return "Scanning Wi-Fi networks...";
    }
    switch (snapshot.connection_state) {
    case WifiConnectionState::Connecting:
        return "Connecting...";
    case WifiConnectionState::Connected:
        return "Connected";
    case WifiConnectionState::Disconnecting:
        return "Disconnecting...";
    case WifiConnectionState::Disconnected:
        return snapshot.network_generation == 0
                   ? "Ready to scan"
                   : (snapshot.network_count == 0 ? "No networks found" : "Choose a Wi-Fi network");
    case WifiConnectionState::Error:
        return "Wi-Fi error";
    case WifiConnectionState::Off:
        return "Wi-Fi is unavailable";
    }
    return "Wi-Fi is unavailable";
}

} // namespace

void WifiSettingsView::init(WifiService &wifi)
{
    wifi_ = &wifi;
}

void WifiSettingsView::show(lv_obj_t *parent, ExitCallback exit_callback, void *exit_context)
{
    exit_callback_ = exit_callback;
    exit_context_ = exit_context;
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(root_, 0, 0);
    UiTheme::apply_page(root_);

    show_networks();
    const esp_err_t scan_result = wifi_->scan();
    if (scan_result != ESP_OK) {
        lv_label_set_text(status_label_, "Unable to start Wi-Fi scan");
    }
}

void WifiSettingsView::hide()
{
    root_ = nullptr;
    status_label_ = nullptr;
    network_list_ = nullptr;
    password_input_ = nullptr;
    if (keyboard_ != nullptr) {
        lv_obj_delete(keyboard_);
        keyboard_ = nullptr;
    }
}

void WifiSettingsView::onSystemEvents(SystemEventMask events)
{
    if (has_system_event(events, SystemEvent::WifiChanged)) {
        update();
    }
}

void WifiSettingsView::rescan_cb(lv_event_t *event)
{
    static_cast<WifiSettingsView *>(lv_event_get_user_data(event))->displayed_generation_ = UINT32_MAX;
    static_cast<WifiSettingsView *>(lv_event_get_user_data(event))->wifi_->scan();
}

void WifiSettingsView::network_selected_cb(lv_event_t *event)
{
    auto *app = static_cast<WifiSettingsView *>(lv_event_get_user_data(event));
    lv_obj_t *button = static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_obj_t *label = lv_obj_get_child(button, 0);
    set_text(app->selected_ssid_, sizeof(app->selected_ssid_), lv_label_get_text(label));
    app->show_password_form();
}

void WifiSettingsView::connect_cb(lv_event_t *event)
{
    auto *app = static_cast<WifiSettingsView *>(lv_event_get_user_data(event));
    app->set_keyboard_visible(false);
    app->wifi_->connect(app->selected_ssid_, lv_textarea_get_text(app->password_input_));
}

void WifiSettingsView::disconnect_cb(lv_event_t *event)
{
    auto *view = static_cast<WifiSettingsView *>(lv_event_get_user_data(event));
    view->wifi_->disconnect();
    view->update();
}

void WifiSettingsView::cancel_password_cb(lv_event_t *event)
{
    auto *app = static_cast<WifiSettingsView *>(lv_event_get_user_data(event));
    app->set_keyboard_visible(false);
    app->show_networks();
}

void WifiSettingsView::password_input_event_cb(lv_event_t *event)
{
    auto *app = static_cast<WifiSettingsView *>(lv_event_get_user_data(event));
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        app->set_keyboard_visible(true);
    } else if (code == LV_EVENT_DEFOCUSED) {
        app->set_keyboard_visible(false);
    }
}

void WifiSettingsView::keyboard_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_READY || lv_event_get_code(event) == LV_EVENT_CANCEL) {
        static_cast<WifiSettingsView *>(lv_event_get_user_data(event))->set_keyboard_visible(false);
    }
}

void WifiSettingsView::update()
{
    if (root_ == nullptr) {
        return;
    }

    WifiSnapshot snapshot{};
    wifi_->snapshot(snapshot);
    if (status_label_ != nullptr) {
        lv_label_set_text(status_label_, wifi_status_text(snapshot));
    }
    if (disconnect_button_ != nullptr) {
        if (snapshot.connection_state == WifiConnectionState::Connected) {
            lv_obj_remove_flag(disconnect_button_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(disconnect_button_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (network_list_ == nullptr || displayed_generation_ == snapshot.network_generation) {
        return;
    }

    lv_obj_clean(network_list_);
    displayed_generation_ = snapshot.network_generation;
    for (uint8_t index = 0; index < snapshot.network_count; ++index) {
        const WifiNetwork &network = snapshot.networks[index];
        if (network.ssid[0] == '\0') {
            continue;
        }
        lv_obj_t *button = lv_button_create(network_list_);
        lv_obj_set_width(button, lv_pct(100));
        lv_obj_set_height(button, 42);
        UiTheme::apply_button(button);
        lv_obj_add_event_cb(button, network_selected_cb, LV_EVENT_CLICKED, this);
        lv_obj_t *label = lv_label_create(button);
        /* The SSID is retained separately so punctuation in the status text is harmless. */
        lv_label_set_text(label, network.ssid);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_t *detail = lv_label_create(button);
        lv_label_set_text_fmt(detail, "%s  %d", network.secured ? "Locked" : "Open", network.rssi);
        UiTheme::apply_muted_text(detail);
        lv_obj_align(detail, LV_ALIGN_RIGHT_MID, -8, 0);
    }
}

void WifiSettingsView::show_networks()
{
    if (keyboard_ != nullptr) {
        lv_obj_delete(keyboard_);
        keyboard_ = nullptr;
    }
    lv_obj_clean(root_);
    password_input_ = nullptr;
    network_list_ = nullptr;
    disconnect_button_ = nullptr;

    lv_obj_t *back = lv_button_create(root_);
    lv_obj_set_size(back, 74, 34);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 10, 7);
    UiTheme::apply_button(back);
    lv_obj_add_event_cb(back, exit_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "Settings");
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text(title, "WI-FI");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    status_label_ = lv_label_create(root_);
    lv_label_set_text(status_label_, "Preparing Wi-Fi...");
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 48);
    UiTheme::apply_muted_text(status_label_);

    lv_obj_t *rescan = lv_button_create(root_);
    lv_obj_set_size(rescan, 74, 34);
    lv_obj_align(rescan, LV_ALIGN_TOP_RIGHT, -10, 7);
    UiTheme::apply_button(rescan);
    lv_obj_add_event_cb(rescan, rescan_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *rescan_label = lv_label_create(rescan);
    lv_label_set_text(rescan_label, "Rescan");
    lv_obj_center(rescan_label);

    disconnect_button_ = lv_button_create(root_);
    lv_obj_set_size(disconnect_button_, lv_pct(94), 34);
    lv_obj_align(disconnect_button_, LV_ALIGN_BOTTOM_MID, 0, -7);
    UiTheme::apply_button(disconnect_button_);
    lv_obj_add_event_cb(disconnect_button_, disconnect_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *disconnect_label = lv_label_create(disconnect_button_);
    lv_label_set_text(disconnect_label, "Disconnect");
    lv_obj_center(disconnect_label);

    network_list_ = lv_obj_create(root_);
    lv_obj_set_size(network_list_, lv_pct(94), 104);
    lv_obj_align(network_list_, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_pad_all(network_list_, 5, 0);
    lv_obj_set_style_pad_gap(network_list_, 5, 0);
    UiTheme::apply_card(network_list_);
    lv_obj_set_flex_flow(network_list_, LV_FLEX_FLOW_COLUMN);
    UiTheme::enable_scroll(network_list_, LV_DIR_VER);
    displayed_generation_ = UINT32_MAX;
    update();
}

void WifiSettingsView::show_password_form()
{
    lv_obj_clean(root_);
    network_list_ = nullptr;

    lv_obj_t *title = lv_label_create(root_);
    lv_label_set_text_fmt(title, "Connect to %s", selected_ssid_);
    lv_obj_set_width(title, lv_pct(92));
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    status_label_ = lv_label_create(root_);
    lv_label_set_text(status_label_, "Enter the password");
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 37);
    UiTheme::apply_muted_text(status_label_);

    password_input_ = lv_textarea_create(root_);
    lv_textarea_set_placeholder_text(password_input_, "Wi-Fi password");
    lv_textarea_set_password_mode(password_input_, true);
    lv_textarea_set_one_line(password_input_, true);
    lv_textarea_set_accepted_chars(password_input_, "0123456789");
    lv_textarea_set_max_length(password_input_, 63);
    lv_obj_set_size(password_input_, lv_pct(92), 44);
    lv_obj_align(password_input_, LV_ALIGN_TOP_MID, 0, 61);
    UiTheme::apply_card(password_input_);
    UiTheme::enable_scroll(password_input_, LV_DIR_HOR);
    lv_obj_add_event_cb(password_input_, password_input_event_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(password_input_, password_input_event_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_event_cb(password_input_, password_input_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *actions = lv_obj_create(root_);
    lv_obj_set_size(actions, lv_pct(92), 44);
    lv_obj_align(actions, LV_ALIGN_TOP_MID, 0, 114);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *cancel = lv_button_create(actions);
    lv_obj_set_size(cancel, 126, 42);
    UiTheme::apply_button(cancel);
    lv_obj_add_event_cb(cancel, cancel_password_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_label_set_text(cancel_label, "Back");
    lv_obj_center(cancel_label);
    lv_obj_t *connect = lv_button_create(actions);
    lv_obj_set_size(connect, 126, 42);
    UiTheme::apply_button(connect, true);
    lv_obj_add_event_cb(connect, connect_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *connect_label = lv_label_create(connect);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);

    /* This layer is above the global bottom-edge gesture target. */
    keyboard_ = lv_keyboard_create(lv_layer_top());
    lv_keyboard_set_textarea(keyboard_, password_input_);
    lv_keyboard_set_mode(keyboard_, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_width(keyboard_, lv_pct(100));
    lv_obj_set_height(keyboard_, 108);
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard_, keyboard_event_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(keyboard_, keyboard_event_cb, LV_EVENT_CANCEL, this);
    set_keyboard_visible(false);
}

void WifiSettingsView::set_keyboard_visible(bool visible)
{
    if (keyboard_ == nullptr) {
        return;
    }
    if (visible) {
        lv_obj_remove_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    }
}

void WifiSettingsView::exit_cb(lv_event_t *event)
{
    auto *view = static_cast<WifiSettingsView *>(lv_event_get_user_data(event));
    view->hide();
    if (view->exit_callback_ != nullptr) {
        view->exit_callback_(view->exit_context_);
    }
}
