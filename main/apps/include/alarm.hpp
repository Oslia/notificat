#ifndef APPS_ALARM_HPP_
#define APPS_ALARM_HPP_

#include "app.hpp"

class Alarm: public App {
public:
	Alarm();
	~Alarm();
	void OnStart() override;
	void OnStop() override;
	void Run() override;
private:
};

#endif	// APPS_ALARM_HPP_