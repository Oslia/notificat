#include "features/weather/ui/WeatherApp.hpp"

#include <cmath>
#include <ctime>

#include "core/AppContext.hpp"
#include "features/weather/WeatherService.hpp"
#include "features/weather/ui/WeatherPresentation.hpp"
#include "ui/UiTheme.hpp"

namespace {

void format_hour(int64_t timestamp, char *buffer, size_t size)
{
    const std::time_t value = static_cast<std::time_t>(timestamp);
    std::tm local{};
    localtime_r(&value, &local);
    std::strftime(buffer, size, "%H:%M", &local);
}

void format_day(int64_t local_date, char *buffer, size_t size)
{
    const std::time_t value = static_cast<std::time_t>(local_date);
    std::tm date{};
    gmtime_r(&value, &date);
    std::strftime(buffer, size, "%a", &date);
}

void add_attribution(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Weather data: Open-Meteo");
    UiTheme::apply_muted_text(label);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -2);
}

lv_obj_t *add_weather_icon(lv_obj_t *parent, WeatherCondition condition,
                           int32_t width, int32_t height)
{
    const char *path = weather_icon_path(condition);
    if (path == nullptr) {
        return nullptr;
    }

    lv_obj_t *icon = lv_image_create(parent);
    lv_obj_set_size(icon, width, height);
    lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_src(icon, path);
    return icon;
}

void add_unavailable_state(lv_obj_t *parent, const WeatherSnapshot &weather)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, weather_status_text(weather));
    lv_obj_set_style_text_color(label, UiTheme::accent(), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -6);
}

} // namespace

void WeatherApp::init(AppContext &context)
{
    context_ = &context;
}

void WeatherApp::onEnter(lv_obj_t *parent)
{
    parent_ = parent;
    render();
}

void WeatherApp::onLeave()
{
    parent_ = nullptr;
    tabview_ = nullptr;
    active_tab_ = 0;
}

void WeatherApp::onSystemEvents(SystemEventMask events)
{
    if (parent_ != nullptr && has_system_event(events, SystemEvent::WeatherChanged)) {
        render();
    }
}

void WeatherApp::render()
{
    if (parent_ == nullptr) {
        return;
    }

    if (tabview_ != nullptr) {
        active_tab_ = lv_tabview_get_tab_active(tabview_);
    }

    lv_obj_clean(parent_);
    const WeatherSnapshot weather = context_->weather.snapshot();

    tabview_ = lv_tabview_create(parent_);
    lv_obj_set_size(tabview_, lv_pct(100), lv_pct(100));
    UiTheme::apply_page(tabview_);
    lv_obj_clear_flag(lv_tabview_get_content(tabview_), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview_);
    lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(tab_bar, UiTheme::surface(), 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tab_bar, UiTheme::border(), 0);
    lv_obj_set_style_border_width(tab_bar, 1, 0);
    lv_obj_set_style_radius(tab_bar, 14, 0);
    lv_obj_set_style_text_color(tab_bar, UiTheme::muted_text(), LV_PART_ITEMS);
    const lv_style_selector_t selected_tab = static_cast<lv_style_selector_t>(LV_PART_ITEMS)
                                             | static_cast<lv_style_selector_t>(LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_bar, UiTheme::accent(), selected_tab);
    lv_obj_set_style_bg_color(tab_bar, UiTheme::surface_tint(), selected_tab);
    lv_obj_set_style_radius(tab_bar, 12, selected_tab);

    lv_obj_t *current_tab = lv_tabview_add_tab(tabview_, "Now");
    lv_obj_clear_flag(current_tab, LV_OBJ_FLAG_SCROLLABLE);
    if (!weather.forecast.current.valid) {
        add_unavailable_state(current_tab, weather);
    } else {
        lv_obj_t *region = lv_label_create(current_tab);
        lv_label_set_text(region, weather.region_name);
        lv_obj_set_width(region, 130);
        lv_label_set_long_mode(region, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(region, LV_ALIGN_TOP_LEFT, 8, 4);
        UiTheme::apply_title(region);

        lv_obj_t *temperature = lv_label_create(current_tab);
        lv_label_set_text_fmt(temperature, "%d C",
                              static_cast<int>(std::lround(weather.forecast.current.temperature_c)));
        lv_obj_set_style_text_font(temperature, &lv_font_montserrat_42, 0);
        lv_obj_set_style_text_color(temperature, UiTheme::text(), 0);
        lv_obj_align(temperature, LV_ALIGN_LEFT_MID, 12, -10);

        lv_obj_t *icon = add_weather_icon(current_tab, weather.forecast.current.condition, 84, 84);
        if (icon != nullptr) {
            lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -10, -8);
        }

        lv_obj_t *details = lv_label_create(current_tab);
        lv_label_set_text_fmt(details, "Feels %d C\nHumidity %u%%  Wind %d km/h",
                              static_cast<int>(std::lround(weather.forecast.current.apparent_temperature_c)),
                              weather.forecast.current.relative_humidity,
                              static_cast<int>(std::lround(weather.forecast.current.wind_speed_kmh)));
        UiTheme::apply_muted_text(details);
        lv_obj_align(details, LV_ALIGN_BOTTOM_LEFT, 8, -22);

        if (weather.status == WeatherStatus::Loading || weather.status == WeatherStatus::Error) {
            lv_obj_t *status = lv_label_create(current_tab);
            lv_label_set_text(status, weather_status_text(weather));
            lv_obj_set_style_text_color(status, UiTheme::accent(), 0);
            lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -8, 4);
        }
    }
    add_attribution(current_tab);

    lv_obj_t *hourly_tab = lv_tabview_add_tab(tabview_, "3-hour");
    lv_obj_clear_flag(hourly_tab, LV_OBJ_FLAG_SCROLLABLE);
    if (weather.forecast.hourly_count == 0) {
        add_unavailable_state(hourly_tab, weather);
    } else {
        lv_obj_t *hourly_list = lv_obj_create(hourly_tab);
        lv_obj_set_size(hourly_list, lv_pct(100), 132);
        lv_obj_align(hourly_list, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_set_style_pad_all(hourly_list, 5, 0);
        lv_obj_set_style_pad_gap(hourly_list, 6, 0);
        UiTheme::apply_card(hourly_list);
        lv_obj_set_flex_flow(hourly_list, LV_FLEX_FLOW_ROW);
        UiTheme::enable_scroll(hourly_list, LV_DIR_HOR);

        for (uint8_t index = 0; index < weather.forecast.hourly_count; ++index) {
            const HourlyWeather &forecast = weather.forecast.hourly[index];
            lv_obj_t *item = lv_obj_create(hourly_list);
            lv_obj_set_size(item, 74, 112);
            lv_obj_set_style_pad_all(item, 4, 0);
            lv_obj_set_style_bg_color(item, UiTheme::surface_tint(), 0);
            lv_obj_set_style_border_color(item, UiTheme::border(), 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_radius(item, 14, 0);
            lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

            char hour[8];
            format_hour(forecast.time, hour, sizeof(hour));
            lv_obj_t *time = lv_label_create(item);
            lv_label_set_text(time, hour);
            lv_obj_set_style_text_color(time, UiTheme::text(), 0);
            lv_obj_align(time, LV_ALIGN_TOP_MID, 0, 1);

            lv_obj_t *icon = add_weather_icon(item, forecast.condition, 38, 38);
            if (icon != nullptr) {
                lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 21);
            }

            lv_obj_t *temperature = lv_label_create(item);
            lv_label_set_text_fmt(temperature, "%d C",
                                  static_cast<int>(std::lround(forecast.temperature_c)));
            lv_obj_set_style_text_color(temperature, UiTheme::text(), 0);
            lv_obj_align(temperature, LV_ALIGN_TOP_MID, 0, 63);

            lv_obj_t *rain = lv_label_create(item);
            lv_label_set_text_fmt(rain, "%u%%", forecast.precipitation_probability);
            UiTheme::apply_muted_text(rain);
            lv_obj_align(rain, LV_ALIGN_TOP_MID, 0, 84);
        }
    }

    lv_obj_t *weekly_tab = lv_tabview_add_tab(tabview_, "7 days");
    lv_obj_clear_flag(weekly_tab, LV_OBJ_FLAG_SCROLLABLE);
    if (weather.forecast.daily_count == 0) {
        add_unavailable_state(weekly_tab, weather);
    } else {
        lv_obj_t *weekly_list = lv_obj_create(weekly_tab);
        lv_obj_set_size(weekly_list, lv_pct(100), 144);
        lv_obj_align(weekly_list, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_set_style_pad_all(weekly_list, 4, 0);
        lv_obj_set_style_pad_gap(weekly_list, 4, 0);
        UiTheme::apply_card(weekly_list);
        lv_obj_set_flex_flow(weekly_list, LV_FLEX_FLOW_COLUMN);
        UiTheme::enable_scroll(weekly_list, LV_DIR_VER);

        for (uint8_t index = 0; index < weather.forecast.daily_count; ++index) {
            const DailyWeather &forecast = weather.forecast.daily[index];
            lv_obj_t *row = lv_obj_create(weekly_list);
            lv_obj_set_size(row, lv_pct(100), 30);
            lv_obj_set_style_pad_hor(row, 7, 0);
            lv_obj_set_style_pad_ver(row, 5, 0);
            lv_obj_set_style_bg_color(row, UiTheme::surface_tint(), 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 10, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            char day[8];
            format_day(forecast.local_date, day, sizeof(day));
            lv_obj_t *day_label = lv_label_create(row);
            lv_label_set_text(day_label, day);
            lv_obj_set_style_text_color(day_label, UiTheme::text(), 0);
            lv_obj_align(day_label, LV_ALIGN_LEFT_MID, 0, 0);

            lv_obj_t *icon = add_weather_icon(row, forecast.condition, 22, 22);
            if (icon != nullptr) {
                lv_obj_align(icon, LV_ALIGN_LEFT_MID, 40, 0);
            }

            lv_obj_t *temperature = lv_label_create(row);
            lv_label_set_text_fmt(temperature, "%d/%d C  %u%%",
                                  static_cast<int>(std::lround(forecast.minimum_temperature_c)),
                                  static_cast<int>(std::lround(forecast.maximum_temperature_c)),
                                  forecast.precipitation_probability);
            UiTheme::apply_muted_text(temperature);
            lv_obj_align(temperature, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }

    lv_tabview_set_active(tabview_, active_tab_, LV_ANIM_OFF);
}
