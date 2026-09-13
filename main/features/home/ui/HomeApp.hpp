#pragma once

#include "ui/app/App.hpp"

class HomeApp final : public App {
public:
    void init(AppContext &context) override;
    void onEnter(lv_obj_t *parent) override;
    void onLeave() override;
    void onSystemEvents(SystemEventMask events) override;

private:
    static void timer_cb(lv_timer_t *timer);
    void refresh_time();
    void refresh_weather();

    lv_obj_t *time_label_ = nullptr;
    lv_obj_t *date_label_ = nullptr;
    lv_obj_t *weather_title_ = nullptr;
    lv_obj_t *weather_icon_ = nullptr;
    lv_obj_t *weather_value_ = nullptr;
    lv_obj_t *weather_detail_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    AppContext *context_ = nullptr;
};
