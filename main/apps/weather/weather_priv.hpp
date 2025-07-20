#ifndef APPS_WEATHER_PRIV_HPP_
#define APPS_WEATHER_PRIV_HPP_

#include <functional>
#include <map>
#include <string>

#include "jsmn.h"
#include "weather.hpp"

#define OPEN_METEO_URL "https://api.open-meteo.com/"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
//#define WEB_URL "https://api.open-meteo.com/v1/forecast?latitude=35.6895&longitude=139.6917&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=Asia/Tokyo"
#define WEB_SERVER "api.open-meteo.com"



namespace Weather {
	class WeatherPriv {
	public:
		WeatherPriv();
		~WeatherPriv();
		void UpdateWeather();
		const lv_image_dsc_t* GetWeatherIcon(WeatherCode code);
		WeatherCode weather_code_for_day[7];
		WeatherCode weather_code_for_3h[8];
	};
}

#endif	// APPS_WEATHER_PRIV_HPP_