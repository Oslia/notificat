#ifndef APPS_WEATHER_HPP_
#define APPS_WEATHER_HPP_

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

	class Weather {
	public:
		Weather();
		~Weather();
		static void* WeatherTask(void*);

	private:
		class WeatherPriv* weather;
	};
}
#endif	// APPS_WEATHER_HPP_