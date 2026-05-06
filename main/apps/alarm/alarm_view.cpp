// alarm_view.cpp

#include <memory>
#include <string>
#include <cstring>

#include "lvgl.h"
#include "app.hpp"

#include "alarm_view.hpp"

LV_IMG_DECLARE(menu_img_release);
LV_IMG_DECLARE(menu_img_press);

namespace Alarm {
    ClockComponent::ClockComponent(lv_obj_t* parent) {
	    lv_obj_t* tile_clock;
        lv_obj_t* tile_set_alarm;
        
        container = lv_obj_create(parent);
        lv_obj_remove_style_all(container);
        lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
        
        clock = lv_label_create(container);
        lv_obj_center(clock);
        lv_obj_set_style_text_font(clock, &lv_font_montserrat_42, 0);
    }


    ClockComponent::~ClockComponent() {

    }


    void ClockComponent::SetTime(const struct tm& tm) {
        char time_buf[64];
        if (tm.tm_hour != hour || tm.tm_min != min) {
            hour = tm.tm_hour;
            min = tm.tm_min;
            sprintf(time_buf, "%2d:%02d", tm.tm_hour, tm.tm_min);
            //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
            lv_label_set_text(clock, time_buf);
        }
    }


    MainView::MainView() {
        //clock = std::make_unique<ClockComponent>(GetContainer());
        tile_view = lv_tileview_create(GetContainer());
        lv_obj_set_size(tile_view, LV_HOR_RES, LV_VER_RES);
        lv_obj_t* tile = lv_tileview_add_tile(tile_view, 0, 0, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
        clock = std::make_unique<ClockComponent>(tile);

        tile = lv_tileview_add_tile(tile_view, 1, 0, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
        list = std::make_unique<AlarmListComponent>(tile);
    }


	MainView::~MainView() {

	}


    void MainView::Draw(const AlarmState& model) {
        clock->SetTime(model.timeinfo);
        list->DrawList(model);
    }


    AlarmListComponent::AlarmListComponent(lv_obj_t* parent) {
        container = lv_obj_create(parent);
        lv_obj_remove_style_all(container);
        lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
        
        list = lv_list_create(container);
        lv_obj_set_size(list, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(list, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(list, 0, 0);

        add_btn = lv_imagebutton_create(container);
        lv_imagebutton_set_src(add_btn, LV_IMGBTN_STATE_RELEASED, NULL, &menu_img_release, NULL);
        lv_imagebutton_set_src(add_btn, LV_IMGBTN_STATE_PRESSED, NULL, &menu_img_press, NULL);
        lv_obj_set_pos(add_btn, LV_HOR_RES - 50, LV_VER_RES - 50);
        lv_obj_set_id(add_btn, (void*)"AlarmAddBtn");
        lv_obj_add_event_cb(add_btn, EventHandler, LV_EVENT_ALL, NULL);
        lv_obj_set_size(add_btn, ALARM_ADD_BTN_WIDTH, ALARM_ADD_BTN_HEIGHT);
        lv_obj_set_pos(add_btn, LV_HOR_RES - ALARM_ADD_BTN_WIDTH - 10, LV_VER_RES - ALARM_ADD_BTN_HEIGHT - 10);
    }


	AlarmListComponent::~AlarmListComponent() {

	}


    void AlarmListComponent::EventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* target = lv_event_get_target_obj(e);
        if (std::string_view("AlarmAddBtn") == std::string_view((char*)lv_obj_get_id(target))) {
            if (code == LV_EVENT_CLICKED) {
                Dispatch("alarm", AppMsg("AlarmAddBtnClicked"));
            }
        }
        if (std::string_view("AlarmListBtn") == std::string_view((char*)lv_obj_get_id(target))) {
            if (code == LV_EVENT_CLICKED) {
                Dispatch("alarm", AppMsg("AlarmListBtnClicked", (int)lv_obj_get_user_data(target)));
            }
        }
    }


    void AlarmListComponent::DrawList(const AlarmState& model) {
        if (memcmp(model.alarm_data, alarm_data, sizeof(alarm_data)) == 0) {
            return;
        }

        memcpy(alarm_data, model.alarm_data, sizeof(alarm_data));
        data_num = model.alarm_num;

        lv_obj_clean(list);  // 既存のリストをクリア


        for (int i = 0; i < model.alarm_num; ++i) {
            const auto& data = model.alarm_data[i];
            lv_obj_t* btn = lv_list_add_button(list, NULL, NULL);
            DrawListButton(btn, data);
            lv_obj_set_id(btn, (void*)("AlarmListBtn"));
            lv_obj_set_user_data(btn, (void*)i);
            lv_obj_add_event_cb(btn, EventHandler, LV_EVENT_ALL, NULL);
        }
    }


    void AlarmListComponent::DrawListButton(lv_obj_t* button, const AlarmData& data) {
        char time_str[8];

        lv_obj_set_layout(button, LV_LAYOUT_NONE);
        lv_obj_set_height(button, 50);
        lv_obj_set_style_pad_all(button, 0, 0);
        lv_obj_set_style_bg_color(button, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);

        snprintf(time_str, sizeof(time_str), "%02d:%02d", data.hour, data.min);
        lv_obj_t* label = lv_label_create(button);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        lv_label_set_text(label, time_str);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
        
        lv_obj_t* repeat = lv_image_create(button);
        lv_image_set_src(repeat, LV_SYMBOL_SHUFFLE);
        lv_obj_align_to(repeat, label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
        lv_obj_set_size(repeat, 30, 30);
        
        CreateIndicator(button, 0, data.days & 0x01);   // Sunday
        CreateIndicator(button, 1, data.days & 0x02);   // Monday
        CreateIndicator(button, 2, data.days & 0x04);   // Tuesday
        CreateIndicator(button, 3, data.days & 0x08);   // Wednesday
        CreateIndicator(button, 4, data.days & 0x10);   // Thursday
        CreateIndicator(button, 5, data.days & 0x20);   // Friday
        CreateIndicator(button, 6, data.days & 0x40);   // Saturday
    }


    void AlarmListComponent::CreateIndicator(lv_obj_t* button, uint8_t day, bool active) {
        #define INDICATOR_SIZE 20
        #define WEEKDAY_COLOR (lv_color_t){255, 255, 255}
        #define SATURDAY_COLOR (lv_color_t){255, 0, 0}
        #define SUNDAY_COLOR (lv_color_t){0, 0, 255}

        lv_obj_t* indicator = lv_obj_create(button);
        lv_obj_remove_style_all(indicator);
        lv_obj_set_style_border_side(indicator, LV_BORDER_SIDE_FULL, 0);
        lv_obj_set_style_border_opa(indicator, LV_OPA_100, 0);
        lv_obj_set_style_border_width(indicator, 1, 0);
        lv_obj_set_size(indicator, INDICATOR_SIZE, INDICATOR_SIZE);
        lv_obj_set_style_radius(indicator, 0, 0);
        if (active) {
            lv_obj_set_style_bg_opa(indicator, LV_OPA_100, 0);
        } else {
            lv_obj_set_style_bg_opa(indicator, LV_OPA_20, 0);
        }

        switch (day) {
            case 0: // Sunday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 7 - 10, 0);
                lv_obj_set_style_bg_color(indicator, SUNDAY_COLOR, 0);
                break;
            case 1: // Monday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 6 - 10, 0);
                lv_obj_set_style_bg_color(indicator, WEEKDAY_COLOR, 0);
                break;
            case 2: // Tuesday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 5 - 10, 0);
                lv_obj_set_style_bg_color(indicator, WEEKDAY_COLOR, 0);
                break;
            case 3: // Wednesday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 4 - 10, 0);
                lv_obj_set_style_bg_color(indicator, WEEKDAY_COLOR, 0);
                break;
            case 4: // Thursday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 3 - 10, 0);
                lv_obj_set_style_bg_color(indicator, WEEKDAY_COLOR, 0);
                break;
            case 5: // Friday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 2 - 10, 0);
                lv_obj_set_style_bg_color(indicator, WEEKDAY_COLOR, 0);
                break;
            case 6: // Saturday
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, - INDICATOR_SIZE * 1 - 10, 0);
                lv_obj_set_style_bg_color(indicator, SATURDAY_COLOR, 0);
                break;
        }
    }


    AlarmEditComponent::AlarmEditComponent(lv_obj_t* parent) {
        container = lv_obj_create(parent);
        lv_obj_remove_style_all(container);
        lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(container, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(container, LV_OPA_COVER, LV_PART_MAIN);
        
        hour_roller = lv_roller_create(container);
        lv_roller_set_options(hour_roller, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_NORMAL);
        lv_obj_set_height(hour_roller, 65);
        lv_obj_align(hour_roller, LV_ALIGN_CENTER, -50, -35);
        min_roller = lv_roller_create(container);
        lv_roller_set_options(min_roller, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_NORMAL);
        lv_obj_set_height(min_roller, 65);
        lv_obj_align(min_roller, LV_ALIGN_CENTER, 50, -35);
        
        save_btn = lv_btn_create(container);
        lv_obj_set_size(save_btn, 100, 40);
        lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_t* save_label = lv_label_create(save_btn);
        lv_label_set_text(save_label, "Save");
        lv_obj_center(save_label);
        lv_obj_set_id(save_btn, (void*)"AlarmSaveBtn");
        lv_obj_add_event_cb(save_btn, EventHandler, LV_EVENT_ALL, NULL);

        for (int i = 0; i < 7; ++i) {
            day_checkbox[i] = lv_obj_create(container);
            lv_obj_remove_style_all(day_checkbox[i]);
            lv_obj_set_style_border_side(day_checkbox[i], LV_BORDER_SIDE_FULL, 0);
            lv_obj_set_style_border_opa(day_checkbox[i], LV_OPA_100, 0);
            lv_obj_set_style_border_width(day_checkbox[i], 1, 0);
            lv_obj_set_size(day_checkbox[i], 30, 30);
            lv_obj_set_style_radius(day_checkbox[i], 0, 0);
            lv_obj_align(day_checkbox[i], LV_ALIGN_BOTTOM_MID, -90 + i * 30, -80);
            lv_obj_set_id(day_checkbox[i], (void*)("AlarmDayCheckbox"));
            lv_obj_set_user_data(day_checkbox[i], (void*)i);
            lv_obj_add_event_cb(day_checkbox[i], EventHandler, LV_EVENT_ALL, NULL);
        }
    }


    AlarmEditComponent::~AlarmEditComponent() {

    }


    void AlarmEditComponent::Draw(const AlarmState& model) {
        if (memcmp(&alarm_data, &model.editing_alarm, sizeof(alarm_data)) == 0) {
            return;
        }
        alarm_data = model.editing_alarm;
        lv_roller_set_selected(hour_roller, alarm_data.hour, LV_ANIM_OFF);
        lv_roller_set_selected(min_roller, alarm_data.min, LV_ANIM_OFF);
        for (int i = 0; i < 7; ++i) {
            if (alarm_data.days & (1 << i)) {
                lv_obj_set_style_bg_opa(day_checkbox[i], LV_OPA_100, 0);
            } else {
                lv_obj_set_style_bg_opa(day_checkbox[i], LV_OPA_20, 0);
            }
        }
    }


    void AlarmEditComponent::EventHandler(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* target = lv_event_get_target_obj(e);
        if (std::string_view("AlarmSaveBtn") == std::string_view((char*)lv_obj_get_id(target))) {
            if (code == LV_EVENT_CLICKED) {
                AppMsg msg("AlarmSave");
                Dispatch("alarm", msg);
            }
        }
        if (std::string_view("AlarmDayCheckbox") == std::string_view((char*)lv_obj_get_id(target))) {
            if (code == LV_EVENT_CLICKED) {
                AppMsg msg("AlarmToggleDay", (int)lv_obj_get_user_data(target));
                Dispatch("alarm", msg);
            }
        }
    }


    AlarmEditView::AlarmEditView() {
        edit_component = std::make_unique<AlarmEditComponent>(GetContainer());
    }


    AlarmEditView::~AlarmEditView() {

    }


    void AlarmEditView::Draw(const AlarmState& model) {
        edit_component->Draw(model);
    }
}

