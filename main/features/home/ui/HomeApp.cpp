#include "features/home/ui/HomeApp.hpp"

#include <ctime>
#include <cmath>

#include "core/AppContext.hpp"
#include "features/weather/WeatherService.hpp"
#include "features/weather/ui/WeatherPresentation.hpp"
#include "platform/time/TimeService.hpp"
#include "ui/UiTheme.hpp"

void HomeApp::init(AppContext &context)
{
    context_ = &context;
}

void HomeApp::onEnter(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "NOTIFICAT  =^.^=");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 10);
    UiTheme::apply_title(title);
    lv_obj_set_style_text_color(title, UiTheme::accent(), 0);

    time_label_ = lv_label_create(parent);
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(time_label_, UiTheme::text(), 0);
    lv_obj_align(time_label_, LV_ALIGN_TOP_MID, 0, 30);

    date_label_ = lv_label_create(parent);
    lv_obj_align(date_label_, LV_ALIGN_TOP_MID, 0, 79);
    UiTheme::apply_muted_text(date_label_);

    lv_obj_t *weather_card = lv_obj_create(parent);
    lv_obj_set_size(weather_card, lv_pct(92), 96);
    lv_obj_align(weather_card, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_pad_all(weather_card, 10, 0);
    UiTheme::apply_card(weather_card);

    weather_icon_ = lv_image_create(weather_card);
    lv_obj_set_size(weather_icon_, 72, 72);
    lv_image_set_inner_align(weather_icon_, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_add_flag(weather_icon_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(weather_icon_, LV_ALIGN_LEFT_MID, 0, 0);

    weather_title_ = lv_label_create(weather_card);
    UiTheme::apply_muted_text(weather_title_);
    lv_obj_align(weather_title_, LV_ALIGN_TOP_LEFT, 82, 5);
    weather_value_ = lv_label_create(weather_card);
    lv_obj_set_style_text_color(weather_value_, UiTheme::accent(), 0);
    lv_obj_align(weather_value_, LV_ALIGN_BOTTOM_LEFT, 82, -7);
    weather_detail_ = lv_label_create(weather_card);
    UiTheme::apply_muted_text(weather_detail_);
    lv_obj_set_width(weather_detail_, 112);
    lv_label_set_long_mode(weather_detail_, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(weather_detail_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(weather_detail_, LV_ALIGN_RIGHT_MID, 0, 0);

    refresh_time();
    refresh_weather();
    timer_ = lv_timer_create(timer_cb, 1000, this);
}

void HomeApp::onLeave()
{
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    time_label_ = nullptr;
    date_label_ = nullptr;
    weather_title_ = nullptr;
    weather_icon_ = nullptr;
    weather_value_ = nullptr;
    weather_detail_ = nullptr;
}

void HomeApp::onSystemEvents(SystemEventMask events)
{
    if (has_system_event(events, SystemEvent::WeatherChanged)) {
        refresh_weather();
    }
}

void HomeApp::timer_cb(lv_timer_t *timer)
{
    static_cast<HomeApp *>(lv_timer_get_user_data(timer))->refresh_time();
}

void HomeApp::refresh_time()
{
    const SystemTime system_time = context_->time.local_time();
    if (!system_time.valid) {
        lv_label_set_text(time_label_, "--:--");
        lv_label_set_text(date_label_, "Set time after connecting to Wi-Fi");
        return;
    }

    char time_text[9];
    char date_text[32];
    std::strftime(time_text, sizeof(time_text), "%H:%M:%S", &system_time.local);
    std::strftime(date_text, sizeof(date_text), "%Y-%m-%d  %a", &system_time.local);
    lv_label_set_text(time_label_, time_text);
    lv_label_set_text(date_label_, date_text);
}

void HomeApp::refresh_weather()
{
    if (weather_title_ == nullptr) {
        return;
    }

    const WeatherSnapshot weather = context_->weather.snapshot();
    lv_label_set_text(weather_title_, weather.region_name);
    if (weather.forecast.current.valid) {
        lv_image_set_src(weather_icon_, weather_icon_path(weather.forecast.current.condition));
        lv_obj_remove_flag(weather_icon_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(weather_value_, "%d C",
                              static_cast<int>(std::lround(weather.forecast.current.temperature_c)));
        lv_label_set_text_fmt(weather_detail_, "Humidity %u%%\nWind %d km/h",
                              weather.forecast.current.relative_humidity,
                              static_cast<int>(std::lround(weather.forecast.current.wind_speed_kmh)));
    } else {
        lv_obj_add_flag(weather_icon_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(weather_value_, "-- C");
        lv_label_set_text(weather_detail_, weather_status_text(weather));
    }
}
