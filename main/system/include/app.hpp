#ifndef APP_HPP_
#define APP_HPP_

#include <string>
#include <lvgl.h>

enum class AppState{
	DESTROYED,
	STOPPED,
	RUNNING,
};

class App {
public:
	std::string name;
	const lv_image_dsc_t* icon;
	virtual lv_obj_t* Run() = 0;
	virtual void OnCreate() {}
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void OnDestroy() {}
private:
};

#endif	// APP_HPP_