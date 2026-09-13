#pragma once

#include "features/weather/WeatherProvider.hpp"

class OpenMeteoProvider final : public WeatherProvider {
public:
    static OpenMeteoProvider &instance();

    esp_err_t fetch(const WeatherLocation &location, WeatherForecastData &forecast) override;

private:
    OpenMeteoProvider() = default;
};
