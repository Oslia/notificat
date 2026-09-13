#pragma once

#include "ui/app/App.hpp"

class WeatherApp final : public App {
public:
    void init(AppContext &context) override;
    void onEnter(lv_obj_t *parent) override;
    void onLeave() override;
    void onSystemEvents(SystemEventMask events) override;

private:
    void render();

    AppContext *context_ = nullptr;
    lv_obj_t *parent_ = nullptr;
    lv_obj_t *tabview_ = nullptr;
    uint32_t active_tab_ = 0;
};
