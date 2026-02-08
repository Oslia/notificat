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
LV_IMG_DECLARE(img_notificat);

SemaphoreHandle_t AppMngPriv::mutex;
AppModel AppMngPriv::model;
bool AppMngPriv::execute_req;
lv_obj_t* scene_storage;

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
	if (nullptr == app) {
		return -1;
	}
	xSemaphoreTake(impl->mutex, portMAX_DELAY);
	impl->model.app_entity.emplace_back(app, AppState::DESTROYED);
	impl->Update(impl->model, AppMsg("AppAdded"));
	ESP_LOGI("AppMng", "%s", "RegisterApp");
	//	impl->list->tile.emplace_back(tile);
	xSemaphoreGive(impl->mutex);

	return 0;
}


void AppMng::Execute(App* app) {
	impl->Execute(app->GetName());
}


AppMngPriv::AppMngPriv() {
	#define PULL_BAR_AREA 20

	mutex = xSemaphoreCreateMutex();
	assert(mutex != NULL);

	screen = lv_screen_active();

	scene_storage = lv_obj_create(NULL);

	main_container = lv_obj_create(screen);
	lv_obj_remove_style_all(main_container);
	lv_obj_set_size(main_container, LV_HOR_RES, LV_VER_RES);
	lv_obj_add_flag(main_container, LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_set_scroll_snap_y(main_container, LV_SCROLL_SNAP_END);

	app_area = lv_obj_create(main_container);
	lv_obj_remove_style_all(app_area);
	lv_obj_set_size(app_area, LV_HOR_RES, LV_VER_RES);
	mascot = lv_image_create(app_area);
	lv_image_set_src(mascot, &img_notificat);

	lv_obj_t* slide_menu_area = lv_obj_create(main_container);
	lv_obj_remove_style_all(slide_menu_area);
	lv_obj_set_size(slide_menu_area, LV_HOR_RES, LV_VER_RES + PULL_BAR_AREA);
	lv_obj_set_pos(slide_menu_area, 0, LV_VER_RES - PULL_BAR_AREA);
    lv_obj_remove_flag(slide_menu_area, LV_OBJ_FLAG_SCROLL_ELASTIC);

	lv_obj_t* list_area = lv_obj_create(main_container);
	lv_obj_remove_style_all(list_area);
	lv_obj_set_size(list_area, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_pos(list_area, 0, LV_VER_RES);
	

	app_list = new AppListComponent(list_area);
//	model.app_entity.emplace_back(&list, AppState::DESTROYED);
//	model.next_app = "app_list";
//	execute_req = true;
	
	model.curr_app = APP_MNG_NULL;
}


void AppMngPriv::Task(void* arg) {
	AppMngPriv* self = static_cast<AppMngPriv*>(arg);
	while(1) {
		self->Manager();
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}


void AppMngPriv::Execute(const std::string& app_name) {
	xSemaphoreTake(mutex, portMAX_DELAY);
	model.next_app = app_name;
	execute_req = true;
	xSemaphoreGive(mutex);
}


void AppMngPriv::Manager(void) {
	lv_obj_t* scn = NULL;
	int next = APP_MNG_NULL;

	bsp_display_lock(0);
	xSemaphoreTake(mutex, portMAX_DELAY);
	
	if (execute_req) {
		execute_req = false;
		next = AppNameToIndex(model.next_app);
	}
	if (APP_MNG_NULL != next) {
		if (next != model.curr_app) {
			if (APP_MNG_NULL != model.curr_app) {
				model.app_entity[model.curr_app].instance->OnStop();
				model.app_entity[model.curr_app].state = AppState::BACKGROUND;
				lv_obj_set_parent(model.app_entity[model.curr_app].instance->GetScene(), scene_storage);
				ESP_LOGI(TAG, "%s set to background", model.app_entity[model.curr_app].instance->name.c_str());
			}
			model.curr_app = next;
			if (AppState::DESTROYED == model.app_entity[model.curr_app].state) {
				model.app_entity[model.curr_app].instance->OnCreate();
				ESP_LOGI(TAG, "Create %s", model.app_entity[model.curr_app].instance->name.c_str());
			}
			model.app_entity[model.curr_app].state = AppState::RUNNING;
			model.app_entity[model.curr_app].instance->OnStart();
			ESP_LOGI(TAG, "%s set to running", model.app_entity[model.curr_app].instance->name.c_str());
			scn = model.app_entity[model.curr_app].instance->scene;
			if (nullptr != scn) {
				lv_obj_set_parent(scn, app_area);
			}
			lv_obj_add_flag(mascot, LV_OBJ_FLAG_HIDDEN);

			next = APP_MNG_NULL;
		}

		lv_obj_scroll_to_y(main_container, 0, LV_ANIM_OFF);
	}
	
	for (AppEntity& app : model.app_entity) {
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
	for (AppEntity& app : model.app_entity) {
		if (app.instance->name == app_name) {
			return index;
		}
		index++;
	}

	return APP_MNG_NULL;
}


void AppMngPriv::Update(AppModel& model, const AppMsg& msg) {
	if (msg.GetName() == std::string_view("AppAdded")) {
		AppEntity entity = model.app_entity.back();
		app_list->AddPage(entity.instance);
	}
}


AppMsg::AppMsg(std::string_view name): name(name) {

}


template <class T>
void AppMsg::SetPayload(T payload) {
	this->payload = payload;
}


std::string_view AppMsg::GetName() const {
	return name;
}


template <class T>
const T* AppMsg::GetPayload() {
	return std::any_cast<T>(&payload);
}


// ====================================================
//	AppList
// ====================================================
AppListComponent::AppListComponent(lv_obj_t* parent) {
	container = lv_obj_create(parent);
	lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES);
	
	tile_view = lv_tileview_create(container);
    lv_obj_add_event_cb(tile_view, EventHandler, LV_EVENT_ALL, NULL);
    lv_obj_set_size(tile_view, LV_HOR_RES, LV_VER_RES);
	page_num = 0;
	current_page = 0;
}


AppListComponent::~AppListComponent() {
	lv_obj_delete(container);
}


void AppListComponent::AddPage(App* app) {
	lv_obj_t* tile = lv_tileview_add_tile(tile_view, page_num, 0, (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT));
	lv_obj_set_user_data(tile, (void*)app);
	lv_obj_add_flag(tile, LV_OBJ_FLAG_EVENT_BUBBLE);
	if (nullptr != app->icon) {
		lv_obj_t* img = lv_image_create(tile);
		lv_image_set_src(img, app->icon);
	} else {
		// set default icon
	}
	page_num++;
}


void AppListComponent::SetPage(int page) {
	if (page > page_num) return;
	
	lv_tileview_set_tile_by_index(tile_view, page, 0, LV_ANIM_OFF);
	current_page = page;
}


void AppListComponent::EventHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* tileview = lv_event_get_target_obj(e);
	App* app = (App*)lv_obj_get_user_data(tileview);
	
    switch(code) {
    case LV_EVENT_PRESSED:
        break;
	case LV_EVENT_PRESSING:
		break;
    case LV_EVENT_CLICKED:
       break;
	case LV_EVENT_RELEASED:
		break;
    case LV_EVENT_LONG_PRESSED:
		AppMngPriv::Execute(app->GetName());
		break;
    case LV_EVENT_LONG_PRESSED_REPEAT:
        break;
    default:
       break;
    }
}


App::App() {
	icon = nullptr;
	scene = lv_obj_create(scene_storage);
	lv_obj_remove_style_all(scene);
	lv_obj_set_size(scene, LV_HOR_RES, LV_VER_RES);
}


App::~App() {
	lv_obj_del(scene);
}


void App::SetName(const std::string& name) {
	this->name = name;
}


const std::string& App::GetName() const {
	return name;
}


lv_obj_t* App::GetScene() const {
	return scene;
}


void App::RequestRun() {
	AppMng& app_mng = AppMng::Instance();
	app_mng.Execute(this);
}
