#ifndef APPS_WEATHER_HPP_
#define APPS_WEATHER_HPP_

#include "app.hpp"

#define WEATHER_REFRASH_INTERVAL_TIME	1800		// s

namespace Weather {
	struct GCS {
		double latitude;
		double longitude;
	};

	enum Location {
		TOKYO,

		LOCATION_NUM
	};

	class Weather: public App {
	public:
		Weather();
		~Weather();
		void OnStart() override;
		void OnStop() override;
		lv_obj_t* Run() override;
	private:
		class WeatherPriv* weather;
		lv_obj_t* screen;
		lv_obj_t* time;
	};
}
#endif	// APPS_WEATHER_HPP_