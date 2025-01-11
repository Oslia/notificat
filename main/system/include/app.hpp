#ifndef APP_HPP_
#define APP_HPP_

#include <string>

enum class AppState{
	DESTROYED,
	STOPPED,
	RUNNING,
};

class App {
public:
	std::string name;
	lv_obj_t * img_icon;
	virtual void Run() {}
	virtual void OnCreate() {}
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void OnDestroy() {}
private:
};

#endif	// APP_HPP_