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

#define WEATHER_CONFIG_FILE_PATH "/spiflash/weather/"
#define WEATHER_CONFIG_FILE	"weather.ini"


namespace Weather {
	typedef struct {
		enum City city;
	} WeatherConfig;
	
	constexpr struct GCS location[City::CITY_NUM] = {
		{35.6895, 139.6917},		// TOKYO
	};
	
	class WeatherPriv {
	public:
		WeatherPriv();
		~WeatherPriv();
		void UpdateWeather();
		const lv_image_dsc_t* GetWeatherIcon(WeatherCode code);
		void SetLocation(City city);
		void SaveConfig();
		void LoadConfig();
		WeatherConfig config;
		WeatherCode weather_code_for_day[7];
		WeatherCode weather_code_for_3h[8];
		int64_t last_update_time;
		jsmn_parser jsmn_config;
	};
}

#endif	// APPS_WEATHER_PRIV_HPP_