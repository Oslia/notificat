#ifndef APP_HPP_
#define APP_HPP_

#include <string>
#include <any>
#include <lvgl.h>

enum class AppState {
	DESTROYED,
	BACKGROUND,
	RUNNING,
};

class App {
public:
	App();
	~App();
	virtual void Run() = 0;
	virtual void OnCreate() {}
	virtual void OnStart() {}
	virtual void OnStop() {}
	virtual void BackgroundRun() {}
	virtual void OnDestroy() {}
	void RequestRun();

	void SetName(const std::string& name);
	const std::string& GetName() const;
	lv_obj_t* GetScene() const;
	const lv_image_dsc_t* icon;

	
private:
	std::string name;
	lv_obj_t* scene;

	friend class AppMngPriv;
};


class AppMsg {
public:
	AppMsg(std::string_view name);

	template <class T>
	void SetPayload(T payload);

	std::string_view GetName() const;

	template <class T>
	const T* GetPayload();

private:
	std::string name;
	std::any payload;
};

#endif	// APP_HPP_