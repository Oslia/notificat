#ifndef APPS_WEATHER_PRIV_HPP_
#define APPS_WEATHER_PRIV_HPP_

#include <functional>
#include <map>
#include <string>

#include "jsmn.h"
#include "weather.hpp"

#define OPEN_METEO_ENDPOINT "https://api.open-meteo.com/v1/forecast?"

namespace Weather {
	using WeatherCode = int;

	class WeatherPriv {
	public:
		WeatherPriv();
		~WeatherPriv();
		WeatherCode GetWeather();
	private:
		
	};
}

#endif	// APPS_WEATHER_PRIV_HPP_