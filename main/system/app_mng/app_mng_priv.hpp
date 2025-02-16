#ifndef APP_MNG_PRIV_HPP_
#define APP_MNG_PRIV_HPP_

#include <vector>
#include "singleton.hpp"
#include "app.hpp"

#define APP_MNG_NULL			-1
#define APP_MNG_APP_LIST		0

#define APP_MNG_SYSTEM_APP_NUM	1

class AppList: public App, public Singleton<AppList> {
public:
	AppList();
	~AppList();
	void OnCreate() override;
	void OnStart() override;
	void OnStop() override;
	void OnDestroy() override;
	lv_obj_t* Run() override;
	lv_obj_t* screen;
	lv_obj_t* tile_view;
	int current_tile;
	std::vector<lv_obj_t*> tile;
private:
	static void BtnPrevEvHandler(lv_event_t* e);
	static void BtnNextEvHandler(lv_event_t* e);
	friend Singleton;
};

class AppMngPriv {
public:
	AppMngPriv();

	static void Task(void*);
	static void SetApp(int app);
	
	static std::vector<std::pair<App*, AppState>> apps;
	static SemaphoreHandle_t mutex;
	static int curr_app;
	static int next_app;

	void Manager();

	AppList* list;

	friend AppList;
};

#endif	// APP_MNG_PRIV_HPP_