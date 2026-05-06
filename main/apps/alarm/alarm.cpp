// alarm.cpp

#include <memory>
#include "lvgl.h"
#include "app_mng.hpp"

#include "alarm_priv.hpp"
#include "alarm.hpp"

LV_IMG_DECLARE(img_alarm);

namespace Alarm {
    Alarm::Alarm() {
        icon = &img_alarm;
        SetName("alarm");
        impl = std::make_unique<AlarmPriv>();
    }


    Alarm::~Alarm() {

    }


    void Alarm::OnCreate() {
        impl->model.alarm_data[0].hour = 8;
        impl->model.alarm_data[0].min = 0;
        impl->model.alarm_data[0].days = 0x3E;  // Monday to Friday

        
        impl->model.alarm_data[1].hour = 8;
        impl->model.alarm_data[1].min = 30;
        impl->model.alarm_data[1].days = 0x7F;  // Everyday

        
        impl->model.alarm_data[2].hour = 9;
        impl->model.alarm_data[2].min = 0;
        impl->model.alarm_data[2].days = 0x01;  // Sunday

        impl->model.alarm_num = 3;
    }


    void Alarm::OnStart() {
        impl->main_view = std::make_unique<MainView>();
        impl->alarm_edit_view = std::make_unique<AlarmEditView>();
        SwitchView(impl->main_view.get());
        impl->main_view->Draw(impl->model);
    }


    void Alarm::Run() {
        static int cnt = 0;
        AppMsg msg("Update");
        if (++cnt > 100) {  // 適当
            cnt = 0;
            Dispatch("alarm", msg);
            if (nullptr != impl->main_view) {
                impl->main_view->Draw(impl->model);
            }
        }
    }


    void Alarm::OnStop() {
        impl->main_view.reset();
    }


    void Alarm::OnDestroy() {
        
    }

    
    void Alarm::Notify(const AppMsg& msg) {
        AlarmViewId prev_view_id = impl->model.view_id;
        impl->Update(impl->model, msg);

        if (prev_view_id != impl->model.view_id) {
            switch (impl->model.view_id) {
                case AlarmViewId::VIEW_MAIN:
                    SwitchView(impl->main_view.get());
                    break;
                case AlarmViewId::VIEW_ALARM_SET:
                    SwitchView(impl->alarm_edit_view.get());
                    break;
                default:
                    break;
            }
        }
        switch (impl->model.view_id) {
            case AlarmViewId::VIEW_MAIN:
                impl->main_view->Draw(impl->model);
                break;
            case AlarmViewId::VIEW_ALARM_SET:
                impl->alarm_edit_view->Draw(impl->model);
                break;
            default:
                break;
        }
    }


    AlarmPriv::AlarmPriv() {
        time(&model.now);
        localtime_r(&model.now, &model.timeinfo);
        model.view_id = AlarmViewId::VIEW_MAIN;
    }


    void AlarmPriv::Update(AlarmState& model, const AppMsg& msg) {
        if (msg.GetName() == std::string_view("Update")) {
            time(&model.now);
            localtime_r(&model.now, &model.timeinfo);
        }
        if (msg.GetName() == std::string_view("AlarmAddBtnClicked")) {
            model.view_id = AlarmViewId::VIEW_ALARM_SET;
            model.editing_alarm.hour = 8;
            model.editing_alarm.min = 0;
            model.editing_alarm.days = 0x00;
            model.editing_alarm.repeat = false;
            model.editing_alarm.activated = false;
            model.editing_index = model.alarm_num;
        }
        if (msg.GetName() == std::string_view("AlarmListBtnClicked")) {
            int index = *msg.GetPayload<int>();
            if (index >= 0 && index < model.alarm_num) {
                model.view_id = AlarmViewId::VIEW_ALARM_SET;
                model.editing_alarm = model.alarm_data[index];
                model.editing_index = index;
            }
        }
        if (msg.GetName() == std::string_view("AlarmSave")) {
            if (model.editing_alarm.hour < 24 && model.editing_alarm.min < 60) {
                if (model.alarm_num < ALARM_MAX_NUM) {
                    model.alarm_data[model.editing_index] = model.editing_alarm;
                }
                model.alarm_num++;
            }
            model.view_id = AlarmViewId::VIEW_MAIN;
        }
        if (msg.GetName() == std::string_view("AlarmToggleDay")) {
            int day = *msg.GetPayload<int>();
            if (day >= 0 && day < 7) {
                model.editing_alarm.days ^= (1 << day);
            }
        }
    }
}
