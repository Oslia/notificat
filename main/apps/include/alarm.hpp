#ifndef APPS_ALARM_HPP_
#define APPS_ALARM_HPP_

#include "app.hpp"

namespace Alarm {
	class Alarm: public App {
	public:
		Alarm();
		~Alarm();
		void OnStart() override;
		void OnStop() override;
		void Run() override;
	private:
		lv_obj_t* clock;
	private:
		time_t now;
	};
}
	
#endif	// APPS_ALARM_HPP_