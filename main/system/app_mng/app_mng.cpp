#include <vector>
#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "app.hpp"
#include "app_mng_priv.hpp"
#include "app_mng.hpp"
#include "bsp/esp-bsp.h"

SemaphoreHandle_t AppMngPriv::mutex = xSemaphoreCreateMutex();
int AppMngPriv::curr_app = APP_MNG_NULL;
int AppMngPriv::next_app = APP_MNG_APP_LIST;
std::vector<std::pair<App*, AppState>> AppMngPriv::apps;

AppMng::AppMng() {
	AppList* list = new AppList;
	assert(list != nullptr);
	RegisterApp(list);
	
	impl = new AppMngPriv;
	assert(impl->mutex != NULL);
	xTaskCreate(impl->Task, "AppMng", 2048, impl, 1, NULL);
}


AppMng::~AppMng() {
	// if not destroyed
	// call OnDestroy
	// delete each app
	delete impl;
}


AppMngPriv::AppMngPriv() {

}

void AppMngPriv::Task(void* arg) {
	AppMngPriv* self = static_cast<AppMngPriv*>(arg);
	while(1) {
		self->Manager();
		vTaskDelay(pdMS_TO_TICKS(35));
	}
}


void AppMngPriv::SetApp(int app) {
	next_app = app;
}

void AppMngPriv::Manager(void) {
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
			int size1 = apps.size();
			AppState dbg = apps[curr_app].second;
			if (AppState::DESTROYED == dbg) {
				apps[curr_app].first->OnCreate();
			}
			apps[curr_app].second = AppState::RUNNING;
			apps[curr_app].first->OnStart();
		}
	}

	if (AppState::RUNNING == apps[curr_app].second) {
		apps[curr_app].first->Run();
	}

	xSemaphoreGive(mutex);
	bsp_display_unlock();
}


int AppMng::RegisterApp(App* app) {
	if (nullptr == app) {
		return -1;
	}
	xSemaphoreTake(impl->mutex, portMAX_DELAY);;
	impl->apps.emplace_back(app, AppState::DESTROYED);
	xSemaphoreGive(impl->mutex);

	return 0;
}


void AppList::OnCreate() {
	tile_view = lv_tileview_create(lv_screen_active());
    /*Tile1: just a label*/
    lv_obj_t * tile1 = lv_tileview_add_tile(tile_view, 0, 0, LV_DIR_BOTTOM);
    lv_obj_t * label = lv_label_create(tile1);
    lv_label_set_text(label, "Scroll down");
    lv_obj_center(label);

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
}


void AppList::OnStart() {
}


void AppList::OnStop() {

}


void AppList::OnDestroy() {

}


void AppList::Run() {

}