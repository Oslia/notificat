#include "lvgl.h"
#include "alarm_priv.hpp"
#include "alarm.hpp"
#include "app_mng.hpp"

LV_IMG_DECLARE(notificat_img);

namespace Alarm {
    Alarm::Alarm() {
        screen = lv_obj_create(NULL);
        clock = lv_label_create(screen);
        lv_obj_center(clock);
        icon = &notificat_img;
        name = "Alarm";
    }


    Alarm::~Alarm() {
        lv_obj_delete(screen);
        lv_obj_delete(clock);
    }


    void Alarm::OnStart() {

    }


    void Alarm::OnStop() {

    }


    lv_obj_t* Alarm::Run() {
        char time_buf[64];
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        sprintf(time_buf, "%d:%d", timeinfo.tm_hour, timeinfo.tm_min);
        //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        lv_label_set_text(clock, time_buf);
        
        return screen;
    }
}