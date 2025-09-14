#include <vector>
#include <functional>
#include <string>

#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "app.hpp"
#include "app_mng_priv.hpp"
#include "app_mng.hpp"
#include "bsp/esp-bsp.h"

#include "esp_log.h"

#define TAG "app_mng"

LV_IMG_DECLARE(menu_img_release);
LV_IMG_DECLARE(menu_img_press);

SemaphoreHandle_t AppMngPriv::mutex;
int AppMngPriv::curr_app = APP_MNG_NULL;
bool AppMngPriv::execute_req = false;
std::string AppMngPriv::next_app;
std::vector<AppEntity> AppMngPriv::app_entity;

AppMng::AppMng() {
	impl = new AppMngPriv;
}


AppMng::~AppMng() {
	// if not destroyed
	// call OnDestroy
	// delete each app
	delete impl;
}


void AppMng::Run() {
	xTaskCreate(impl->Task, "AppMng", 4096, impl, 1, NULL);
}


int AppMng::RegisterApp(App* app) {
	lv_obj_t* img;
	lv_obj_t* tile;
	if (nullptr == app) {
		return -1;
	}
	xSemaphoreTake(impl->mutex, portMAX_DELAY);
	impl->app_entity.emplace_back(app, AppState::DESTROYED);
	ESP_LOGI("AppMng", "%s", "RegisterApp");
	tile = lv_tileview_add_tile(impl->list->tile_view, impl->app_entity.size() - 2, 0, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
	lv_obj_set_user_data(tile, (void*)&app->name);
	lv_obj_add_flag(tile, LV_OBJ_FLAG_EVENT_BUBBLE);
	impl->list->tile.emplace_back(tile);
	if (nullptr != app->icon) {
		img = lv_image_create(tile);
		lv_image_set_src(img, app->icon);
	} else {
		// set default icon
	}
	xSemaphoreGive(impl->mutex);

	return 0;
}


void AppMngPriv::BtnMenuEventHandler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
		AppMngPriv::Execute("app_list");
    }
}


AppMngPriv::AppMngPriv() {
	mutex = xSemaphoreCreateMutex();
	assert(mutex != NULL);
	lv_obj_t* top = lv_layer_top();

	btn_menu = lv_imagebutton_create(top);
	lv_imagebutton_set_src(btn_menu, LV_IMGBTN_STATE_RELEASED, NULL, &menu_img_release, NULL);
	lv_imagebutton_set_src(btn_menu, LV_IMGBTN_STATE_PRESSED, NULL, &menu_img_press, NULL);
    lv_obj_add_event_cb(btn_menu, BtnMenuEventHandler, LV_EVENT_ALL, NULL);

	lv_obj_set_size(btn_menu, APP_LIST_MENU_BTN_WIDTH, APP_LIST_MENU_BTN_HEIGHT);
	lv_obj_set_pos(btn_menu, LV_HOR_RES - APP_LIST_MENU_BTN_WIDTH - 10, LV_VER_RES - APP_LIST_MENU_BTN_HEIGHT - 10);
	lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_HIDDEN);

	list = &AppList::Instance();
	app_entity.emplace_back(list, AppState::DESTROYED);
	next_app = "app_list";
	execute_req = true;
}


void AppMngPriv::Task(void* arg) {
	AppMngPriv* self = static_cast<AppMngPriv*>(arg);
	while(1) {
		self->Manager();
		vTaskDelay(pdMS_TO_TICKS(35));
	}
}


void AppMngPriv::Execute(std::string app_name) {
	xSemaphoreTake(mutex, portMAX_DELAY);
	next_app = app_name;
	execute_req = true;
	xSemaphoreGive(mutex);
}


void AppMngPriv::Manager(void) {
	lv_obj_t* scr = NULL;
	int next = APP_MNG_NULL;

	bsp_display_lock(0);
	xSemaphoreTake(mutex, portMAX_DELAY);
	
	if (execute_req) {
		execute_req = false;
		next = AppNameToIndex(next_app);
	}
	if (APP_MNG_NULL != next) {
		if (next != curr_app) {
			if (APP_MNG_NULL != curr_app) {
				app_entity[curr_app].instance->OnStop();
				app_entity[curr_app].state = AppState::BACKGROUND;
				ESP_LOGI(TAG, "%s set to background", app_entity[curr_app].instance->name.c_str());
			}
			curr_app = next;
			if (AppState::DESTROYED == app_entity[curr_app].state) {
				app_entity[curr_app].instance->OnCreate();
				ESP_LOGI(TAG, "Create %s", app_entity[curr_app].instance->name.c_str());
			}
			app_entity[curr_app].state = AppState::RUNNING;
			app_entity[curr_app].instance->OnStart();
			ESP_LOGI(TAG, "%s set to running", app_entity[curr_app].instance->name.c_str());
			scr = app_entity[curr_app].instance->screen;
			if (nullptr != scr) {
				lv_scr_load(scr);
			}
			
			if (APP_MNG_APP_LIST == next) {
				lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_HIDDEN);
			} else {
				lv_obj_remove_flag(btn_menu, LV_OBJ_FLAG_HIDDEN);
			}

			next = APP_MNG_NULL;
		}
	}
	
	for (AppEntity& app : app_entity) {
		if (AppState::BACKGROUND == app.state) {
			app.instance->BackgroundRun();
		} else if (AppState::RUNNING == app.state) {
			app.instance->Run();
		}
	}

	xSemaphoreGive(mutex);
	bsp_display_unlock();
}


int AppMngPriv::AppNameToIndex(const std::string& app_name) {
	int index = 0;
	for (AppEntity& app : app_entity) {
		if (app.instance->name == app_name) {
			return index;
		}
		index++;
	}

	return APP_MNG_NULL;
}


AppList::AppList() {
	screen = lv_obj_create(NULL);
	name = "app_list";
	
	tile_view = lv_tileview_create(screen);
    lv_obj_add_event_cb(tile_view, EventHandler, LV_EVENT_ALL, NULL);
    lv_obj_set_size(tile_view, LV_HOR_RES, LV_VER_RES);
}


AppList::~AppList() {
	// delete tile, tile view
}


void AppList::OnStart(void) {
	lv_tileview_set_tile_by_index(tile_view, 0, 0, LV_ANIM_OFF);
}


void AppList::OnStop(void) {

}


void AppList::Run(void) {
	return;
}


void AppList::EventHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* tileview = lv_event_get_target_obj(e);
	std::string* app_name = (std::string*)lv_obj_get_user_data(tileview);
    switch(code) {
    case LV_EVENT_PRESSED:
        break;
	case LV_EVENT_PRESSING:

		break;
    case LV_EVENT_CLICKED:

       break;
	case LV_EVENT_RELEASED:
		break;
    case LV_EVENT_LONG_PRESSED: {
		AppMngPriv::Execute(*app_name);
		break;
	}
    case LV_EVENT_LONG_PRESSED_REPEAT:

        break;
    default:
       break;
    }
}
