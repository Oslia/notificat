#pragma once

#include "features/weather/WeatherService.hpp"

const char *weather_icon_path(WeatherCondition condition);
const char *weather_status_text(const WeatherSnapshot &weather);
