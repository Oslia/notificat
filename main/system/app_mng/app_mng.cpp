#include <vector>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "app.hpp"
#include "app_mng_priv.hpp"
#include "app_mng.hpp"
#include "bsp/esp-bsp.h"

#include "esp_log.h"

SemaphoreHandle_t AppMngPriv::mutex = xSemaphoreCreateMutex();
int AppMngPriv::curr_app = APP_MNG_NULL;
int AppMngPriv::next_app = APP_MNG_APP_LIST;
std::vector<std::pair<App*, AppState>> AppMngPriv::apps;

AppMng::AppMng() {
	impl = new AppMngPriv;
	assert(impl->mutex != NULL);
}


AppMng::~AppMng() {
	// if not destroyed
	// call OnDestroy
	// delete each app
	delete impl;
}


AppMngPriv::AppMngPriv() {
	list = &AppList::Instance();
	apps.emplace_back(list, AppState::DESTROYED);
}

void AppMngPriv::Task(void* arg) {
	AppMngPriv* self = static_cast<AppMngPriv*>(arg);
	while(1) {
		self->Manager();
		vTaskDelay(pdMS_TO_TICKS(35));
	}
}


void AppMngPriv::Execute(int app) {
	next_app = app;
}


void AppMngPriv::Manager(void) {
	lv_obj_t* scr;
	bsp_display_lock(0);
	xSemaphoreTake(mutex, portMAX_DELAY);
	
	if (APP_MNG_NULL != next_app) {
		if (next_app != curr_app) {
			if (APP_MNG_NULL != curr_app) {
				apps[curr_app].first->OnStop();
				apps[curr_app].second = AppState::STOPPED;
			}
			curr_app = next_app;
			next_app = APP_MNG_NULL;
			if (AppState::DESTROYED == apps[curr_app].second) {
				apps[curr_app].first->OnCreate();
			}
			apps[curr_app].second = AppState::RUNNING;
			apps[curr_app].first->OnStart();
		}
	}

	if (AppState::RUNNING == apps[curr_app].second) {
		scr = apps[curr_app].first->Run();
		if (nullptr != scr) {
			lv_scr_load(scr);
		}
	}

	xSemaphoreGive(mutex);
	bsp_display_unlock();
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
	impl->apps.emplace_back(app, AppState::DESTROYED);
	ESP_LOGI("AppMng", "%s", "RegisterApp");
	tile = lv_tileview_add_tile(impl->list->tile_view, impl->apps.size() - 2, 0, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
	lv_obj_set_user_data(tile, (void*)app->name.c_str());
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


AppList::AppList() {
	screen = lv_obj_create(NULL);
	tile_view = lv_tileview_create(screen);
    lv_obj_add_event_cb(tile_view, EventHandler, LV_EVENT_ALL, NULL);
}


AppList::~AppList() {
	// delete tile, tile view
}


void AppList::OnCreate() {
    /*Tile1: just a label*/
    //lv_obj_t * tile1 = lv_tileview_add_tile(tile_view, 0, 0, LV_DIR_BOTTOM);
    //lv_obj_t * img = lv_image_create(tile1);
    //lv_obj_center(img);
#if 0
    /*Tile2: a button*/
    lv_obj_t * tile2 = lv_tileview_add_tile(tile_view, 0, 1, (lv_dir_t)(LV_DIR_TOP | LV_DIR_RIGHT));

    lv_obj_t * btn = lv_button_create(tile2);

    label = lv_label_create(btn);
    lv_label_set_text(label, "Scroll up or right");

    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(btn);

    /*Tile3: a list*/
    lv_obj_t * tile3 = lv_tileview_add_tile(tile_view, 1, 1, LV_DIR_LEFT);
    lv_obj_t * list = lv_list_create(tile3);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
#endif
}


void AppList::OnStart() {

}


void AppList::OnStop() {
	
}


void AppList::OnDestroy() {

}


lv_obj_t* AppList::Run() {
	return screen;
}


void AppList::EventHandler(lv_event_t* e) {
	AppList& app_list = AppList::Instance();
    lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* widget = lv_event_get_target_obj(e);
	char* app_name = (char*)lv_obj_get_user_data(widget);

    switch(code) {
        case LV_EVENT_PRESSED:

            break;
        case LV_EVENT_CLICKED:

            break;
        case LV_EVENT_LONG_PRESSED:
			printf("widget:%s\n", app_name);
			AppMngPriv::Execute(app_list.current_tile + APP_MNG_SYSTEM_APP_NUM);
            break;
        case LV_EVENT_LONG_PRESSED_REPEAT:

            break;
        default:
            break;
    }
}