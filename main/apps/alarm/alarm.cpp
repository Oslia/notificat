#include "lvgl.h"
#include "alarm_priv.hpp"
#include "alarm.hpp"
#include "app_mng.hpp"

Alarm::Alarm() {
    AppMng& app_mng_hdlr = AppMng::Instance();
    app_mng_hdlr.RegisterApp(this);
}


Alarm::~Alarm() {

}


void Alarm::OnStart() {

}


void Alarm::OnStop() {

}


void Alarm::Run() {

}