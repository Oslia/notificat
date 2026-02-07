#include <memory>
#include <type_traits>
#include "lvgl.h"
#include "alarm_priv.hpp"
#include "alarm.hpp"
#include "app_mng.hpp"

LV_IMG_DECLARE(img_notificat);

namespace Alarm {
    Alarm::Alarm() {
        screen = lv_obj_create(NULL);
        icon = &img_notificat;
        SetName("alarm");
        v = new ClockComponent(screen);
    }


    Alarm::~Alarm() {
		lv_obj_delete(screen);
        delete v;
    }


    void Alarm::OnCreate() {

    }


    void Alarm::OnStart() {

    }


    void Alarm::Run() {
        Update(model, Msg("Update"));
    }


    void Alarm::OnStop() {

    }


    void Alarm::OnDestroy() {

    }


    void Alarm::Update(AlarmState& model, const Msg& msg) {
        if (msg.GetName() == std::string_view("Update")) {
            time(&model.now);
            localtime_r(&model.now, &model.timeinfo);
            v->SetTime(model.timeinfo);
        }
    }

	Msg::Msg(std::string_view name): name(name) {

    }


    template <class T>
    void Msg::SetPayload(T payload) {
        this->payload = payload;
    }


    std::string_view Msg::GetName() const {
        return name;
    }
    

    template <class T>
    const T* Msg::GetPayload() {
        return std::any_cast<T>(&payload);
    }


    AlarmState::AlarmState() {
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    
    ClockComponent::ClockComponent(lv_obj_t* parent) {
	    lv_obj_t* tile_clock;
        lv_obj_t* tile_set_alarm;
        
        container = lv_obj_create(parent);
        lv_obj_remove_style_all(container);
        lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(container, lv_color_black(), 0);
        clock = lv_label_create(container);
        lv_obj_center(clock);
        lv_obj_set_style_text_font(clock, &lv_font_montserrat_42, 0);
    }


    ClockComponent::~ClockComponent() {
        lv_obj_delete(container);
    }


    void ClockComponent::SetTime(struct tm& tm) {
        char time_buf[64];
        sprintf(time_buf, "%2d:%02d", tm.tm_hour, tm.tm_min);
        //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        lv_label_set_text(clock, time_buf);
    }
}
