#include "lvgl.h"
#include "alarm_priv.hpp"
#include "alarm.hpp"
#include "app_mng.hpp"

LV_IMG_DECLARE(img_notificat);

namespace Alarm {
    Alarm::Alarm() {
        screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        clock = lv_label_create(screen);
        lv_obj_center(clock);
        icon = &img_notificat;
        name = "alarm";
        lv_obj_set_style_text_font(clock, &lv_font_montserrat_42, 0);
    }


    Alarm::~Alarm() {
        lv_obj_delete(screen);
        lv_obj_delete(clock);
    }


    void Alarm::OnStart() {

    }


    void Alarm::OnStop() {

    }


    void Alarm::Run() {
        char time_buf[64];
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        sprintf(time_buf, "%2d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        lv_label_set_text(clock, time_buf);
        
        return;
    }
}