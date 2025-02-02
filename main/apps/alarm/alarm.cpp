#include "lvgl.h"
#include "alarm_priv.hpp"
#include "alarm.hpp"
#include "app_mng.hpp"

Alarm::Alarm() {
    screen = lv_obj_create(NULL);
    time = lv_label_create(screen);
    icon = "A:rsc/notificat.bmp";
    name = "Alarm";
}


Alarm::~Alarm() {
    lv_obj_delete(screen);
    lv_obj_delete(time);
}


void Alarm::OnStart() {

}


void Alarm::OnStop() {

}


lv_obj_t* Alarm::Run() {
    lv_label_set_text(time, "17:00");
    return screen;
}