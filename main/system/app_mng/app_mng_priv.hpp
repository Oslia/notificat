#ifndef APP_MNG_PRIV_HPP_
#define APP_MNG_PRIV_HPP_

#include <vector>
#include "singleton.hpp"
#include "app.hpp"

#define APP_MNG_NULL				-1
#define APP_MNG_APP_LIST			0

#define APP_MNG_APP_LIST_BAR_SIZE	10

#define APP_LIST_MENU_BTN_WIDTH		75
#define APP_LIST_MENU_BTN_HEIGHT	75

class AppMngPriv;
struct AppEntity {
	App* instance;
	AppState state;
};


class AppListComponent {
public:
	static void EventHandler(lv_event_t* e);

	AppListComponent(lv_obj_t* parent);

	~AppListComponent();

	void AddPage(App* app);
	
	void SetPage(int page);


private:
	lv_obj_t* container;
	lv_obj_t* tile_view;
	int page_num;
	int current_page;
};


class AppModel {
public:
	std::vector<AppEntity> app_entity;
	int curr_app;
	std::string next_app;
};


class AppMngPriv {
public:
	AppMngPriv();

	static void Task(void*);

	static void Execute(const std::string& app_name);

	void Manager();

	int AppNameToIndex(const std::string& app_name);

	static SemaphoreHandle_t mutex;		// For the case of app is run on seperate task
	static bool execute_req;

	static AppModel model;

	void Update(AppModel& model, const AppMsg& msg);
private:
	int current_tile;
	AppListComponent* app_list;
	lv_obj_t* screen;
	lv_obj_t* main_container;
	lv_obj_t* app_area;
	lv_obj_t* mascot;
};

#endif	// APP_MNG_PRIV_HPP_