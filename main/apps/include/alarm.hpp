#ifndef APPS_ALARM_HPP_
#define APPS_ALARM_HPP_

#include "app.hpp"

class Alarm: public App {
public:
	Alarm();
	~Alarm();
	void OnStart() override;
	void OnStop() override;
	lv_obj_t* Run() override;
private:
	lv_obj_t* screen;
	lv_obj_t* time;
};

#endif	// APPS_ALARM_HPP_