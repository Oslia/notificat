#pragma once

#include "esp_err.h"
#include "features/weather/WeatherTypes.hpp"

struct WeatherLocation {
    double latitude;
    double longitude;
};

class WeatherProvider {
public:
    virtual ~WeatherProvider() = default;
    virtual esp_err_t fetch(const WeatherLocation &location, WeatherForecastData &forecast) = 0;
};
