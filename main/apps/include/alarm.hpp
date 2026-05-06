#ifndef APPS_ALARM_HPP_
#define APPS_ALARM_HPP_

#include <any>
#include <memory>

#include "app.hpp"

namespace Alarm {
	class ButtonClick {
	public:
	};


	class Updates {
	public:
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

		void Notify(const AppMsg& msg) override;

		void Update(const AppMsg& msg);


	private:
		std::unique_ptr<class AlarmPriv> impl;
	};
}
	
#endif	// APPS_ALARM_HPP_
