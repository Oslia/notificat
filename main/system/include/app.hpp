#ifndef APP_HPP_
#define APP_HPP_

#include <string>
#include <lvgl.h>

enum class AppState {
	DESTROYED,
	BACKGROUND,
	RUNNING,
};

class App {
public:
	App();
	virtual ~App() {}
	virtual void Run() = 0;
	virtual void OnCreate() {}
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void BackgroundRun() {}
	virtual void OnDestroy() {}
	void RequestRun();

	void SetName(const std::string& name);
	const std::string& GetName() const;

	const lv_image_dsc_t* icon;

protected:
	std::string name;
	lv_obj_t* screen;

	friend class AppMngPriv;
};

#endif	// APP_HPP_