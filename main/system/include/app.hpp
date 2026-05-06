#ifndef APP_HPP_
#define APP_HPP_

#include <string>
#include <any>
#include <vector>
#include <memory>

#include <lvgl.h>

class AppMsg;

lv_obj_t* CreateContainer();

void Dispatch(std::string_view app_name, const AppMsg& msg);

enum class AppState {
	DESTROYED,
	BACKGROUND,
	RUNNING,
};

class AppMsg {
public:
	AppMsg(std::string_view name);
	AppMsg(std::string_view name, std::any payload);
	template <class T>
	void SetPayload(T payload);

	std::string_view GetName() const;

	template <class T>
	const T* GetPayload() const{
		return std::any_cast<T>(&payload);
	};
	

private:
	std::string name;
	std::any payload;
};


template <class Model>
class View {
public:
	View() {
        container = CreateContainer();
	}

	~View() {
		lv_obj_delete(container);
	}

	virtual void Draw(const Model& model) {};
	lv_obj_t* GetContainer() {
		return container;
	}

private:
	lv_obj_t* container;
};


template <class Model>
class State {
public:
	~State() = default;
	virtual void Update(const AppMsg& msg);
	void Link(std::unique_ptr<View<Model>> view);

private:
	Model model;
	std::vector<std::unique_ptr<View<Model>>> views;
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
	template <class Model>
	void SwitchView(View<Model>* view) {
		if (nullptr != current_view) {
			lv_obj_set_parent(current_view, GetSceneStorage());
		}
		lv_obj_set_parent(view->GetContainer(), scene);
	}

	void SetName(const std::string& name);
	const std::string& GetName() const;
	lv_obj_t* GetScene() const;
	virtual void Notify(const AppMsg& msg) {};
	const lv_image_dsc_t* icon;

private:
	std::string name;
	lv_obj_t* scene;

	lv_obj_t* current_view;
	friend class AppMngPriv;
	static lv_obj_t* GetSceneStorage();
};

#endif	// APP_HPP_
