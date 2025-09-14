#include "lvgl.h"
#include "alarm_priv.hpp"
#include "alarm.hpp"
#include "app_mng.hpp"

LV_IMG_DECLARE(img_notificat);

namespace Alarm {
    Alarm::Alarm() {
        icon = &img_notificat;
        name = "alarm";
    }


    Alarm::~Alarm() {
		if (nullptr != screen) {
			lv_obj_delete(screen);
		}
    }


    void Alarm::OnStart() {
        screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        clock = lv_label_create(screen);
        lv_obj_center(clock);
        lv_obj_set_style_text_font(clock, &lv_font_montserrat_42, 0);
    }


    void Alarm::OnStop() {
        lv_obj_delete(screen);
        screen = nullptr;
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