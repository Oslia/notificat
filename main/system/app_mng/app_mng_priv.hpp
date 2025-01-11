#ifndef APP_MNG_PRIV_HPP_
#define APP_MNG_PRIV_HPP_

#include <vector>
#include "app.hpp"

#define APP_MNG_NULL			-1
#define APP_MNG_APP_LIST		0

#define APP_MNG_SYSTEM_APP_NUM	1

class AppList: public App {
public:
	void OnCreate() override;
	void OnStart() override;
	void OnStop() override;
	void OnDestroy() override;
	void Run() override;
private:
	lv_obj_t* tile_view;
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
	friend AppList;
};

#endif	// APP_MNG_PRIV_HPP_