#ifndef APPS_WEATHER_HPP_
#define APPS_WEATHER_HPP_

#include "app.hpp"

#define WEATHER_REFRASH_INTERVAL_TIME	1800		// s

namespace Weather {
	enum WeatherCode: int {
		NONE = -1,
		SUNNY = 0,
		MAINLY_SUNNY = 1,
		PARTLY_CLOUDY = 2,
		CLOUDY = 3,
		FOGGY = 45,
		RIME_FOG = 48,
		LIGHT_DRIZZLE = 51,
		DRIZZLE = 53,
		HEAVY_DRIZZLE = 55,
		LIGHT_FREEZING_DRIZZLE = 56,
		FREEZING_DRIZZLE = 57,
		LIGHT_RAIN = 61,
		RAIN = 63,
		HEAVY_RAIN = 65,
		LIGHT_FREEZING_RAIN = 66,
		FREEZING_RAIN = 67,
		LIGHT_SNOW = 71,
		SNOW = 73,
		HEAVY_SNOW = 75,
		SNOW_GRAINS = 77,
		LIGHT_SHOWERS = 80,
		SHOWERS = 81,
		HEAVY_SHOUWERS = 82,
		LIGHT_SNOW_SHOWERS = 85,
		SNOW_SHOWERS = 86,
		THUNDERSTORM = 95,
		LIGHT_THUNDERSTORMS_WITH_HAIL = 96,
		THUNDERSTORM_WITH_HAIL = 99,
	};

	struct GCS {
		double latitude;
		double longitude;
	};

	enum City {
		DEFAULT_CITY,
		TOKYO = DEFAULT_CITY,

		CITY_NUM
	};

	class Weather: public App {
	public:
		Weather();
		~Weather();
		void OnStart() override;
		void OnStop() override;
		void Run() override;
	private:
		class WeatherPriv* weather;
		lv_obj_t* container_weather_now;
		lv_obj_t* container_weather_3h;
	};
}
#endif	// APPS_WEATHER_HPP_