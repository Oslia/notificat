#include "features/weather/ui/WeatherPresentation.hpp"

const char *weather_icon_path(WeatherCondition condition)
{
    switch (condition) {
    case WeatherCondition::Clear:
        return "P:/spiflash/weather/Sunny.png";
    case WeatherCondition::PartlyCloudy:
        return "P:/spiflash/weather/Partly Cloudy.png";
    case WeatherCondition::Overcast:
        return "P:/spiflash/weather/Cloudy.png";
    case WeatherCondition::Fog:
        return "P:/spiflash/weather/Foggy.png";
    case WeatherCondition::Drizzle:
        return "P:/spiflash/weather/Drizzle.png";
    case WeatherCondition::Rain:
    case WeatherCondition::Shower:
        return "P:/spiflash/weather/Rain.png";
    case WeatherCondition::Snow:
        return "P:/spiflash/weather/Snow.png";
    case WeatherCondition::Thunderstorm:
        return "P:/spiflash/weather/Thunderstorm.png";
    case WeatherCondition::Unknown:
        return nullptr;
    }
    return nullptr;
}

const char *weather_status_text(const WeatherSnapshot &weather)
{
    switch (weather.status) {
    case WeatherStatus::NotLoaded:
        return "Weather data is not loaded";
    case WeatherStatus::WaitingForNetwork:
        return "Waiting for Wi-Fi";
    case WeatherStatus::Loading:
        return "Updating weather...";
    case WeatherStatus::Available:
        return "Weather updated";
    case WeatherStatus::Error:
        return "Unable to update weather";
    }
    return "Weather unavailable";
}
