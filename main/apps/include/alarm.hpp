#ifndef APPS_ALARM_HPP_
#define APPS_ALARM_HPP_

#include <any>
#include "app.hpp"

namespace Alarm {
	class Msg {
	public:
		Msg(std::string_view name);

		template <class T>
		void SetPayload(T payload);

		std::string_view GetName() const;

		template <class T>
		const T* GetPayload();

	private:
		std::string name;
		std::any payload;
	};


	class ButtonClick {
	public:
	};


	class Updates {
	public:
	};


	class AlarmState {
	public:
		AlarmState();
		time_t now;
        struct tm timeinfo;
	};

	
	class ClockComponent {
	public:
		ClockComponent(lv_obj_t* parent);
		~ClockComponent();
		void SetTime(struct tm& tm);


	private:
		lv_obj_t* container;
		lv_obj_t* clock;
		lv_obj_t* tile_view;
	};


	class Alarm: public App {
	public:
		Alarm();

		~Alarm();

		void OnCreate() override;

		void OnStart() override;

		void Run() override;

		void OnStop() override;

		void OnDestroy() override;

		void Update(AlarmState& model, const Msg& msg);

	private:
		ClockComponent* v;
		AlarmState model;
	};
}
	
#endif	// APPS_ALARM_HPP_
