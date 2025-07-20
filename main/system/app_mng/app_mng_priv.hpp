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

class AppList: public App, Singleton<AppList> {
public:
	AppList();
	~AppList();
	void OnStart() override;
	void OnStop() override;
	void Run() override;
	lv_obj_t* tile_view;
	int current_tile;
	std::vector<lv_obj_t*> tile;
private:
	static void EventHandler(lv_event_t* e);
	friend AppMngPriv;
	friend Singleton;
};

class AppMngPriv {
public:
	AppMngPriv();

	static void Task(void*);
	static void Execute(std::string app_name);
	static void BtnMenuEventHandler(lv_event_t * e);

	static std::vector<std::pair<App*, AppState>> apps;
	static SemaphoreHandle_t mutex;		// For the case of app is run on seperate task
	static int curr_app;
	static std::string next_app;
	static bool execute_req;
	
	lv_obj_t* btn_menu;
	void Manager();
	int AppNameToIndex(const std::string& app_name);
	AppList* list;

	friend AppList;
};

#endif	// APP_MNG_PRIV_HPP_