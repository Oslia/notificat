#ifndef APP_HPP_
#define APP_HPP_

#include <string>
#include <lvgl.h>

enum class AppState {
	DESTROYED,
	BACKGROUND,
	RUNNING,
};

enum class AppPriority {
	PRIORITY1,		// LOW
	PRIORITY2,
	PRIORITY3,
	PRIORITY4,
	PRIORITY5,		// HIGH
};

class App {
public:
	std::string name;
	const lv_image_dsc_t* icon;
	lv_obj_t* screen;
	AppPriority priority;

	App();
	virtual ~App() {}
	virtual void Run() = 0;
	virtual void OnCreate() {}
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void BackgroundRun() {}
	virtual void OnDestroy() {}
	void RequestRun();
private:
};

#endif	// APP_HPP_