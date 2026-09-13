#pragma once

#include <cstdint>

constexpr uint8_t kWeatherHourlyCount = 8;
constexpr uint8_t kWeatherDailyCount = 7;

enum class WeatherCondition : uint8_t {
    Unknown,
    Clear,
    PartlyCloudy,
    Overcast,
    Fog,
    Drizzle,
    Rain,
    Snow,
    Shower,
    Thunderstorm,
};

struct CurrentWeather {
    bool valid = false;
    int64_t time = 0;
    float temperature_c = 0;
    float apparent_temperature_c = 0;
    float wind_speed_kmh = 0;
    uint8_t relative_humidity = 0;
    WeatherCondition condition = WeatherCondition::Unknown;
};

struct HourlyWeather {
    int64_t time = 0;
    float temperature_c = 0;
    uint8_t precipitation_probability = 0;
    WeatherCondition condition = WeatherCondition::Unknown;
};

struct DailyWeather {
    // Local calendar date represented as a UTC timestamp for formatting with gmtime_r().
    int64_t local_date = 0;
    float minimum_temperature_c = 0;
    float maximum_temperature_c = 0;
    uint8_t precipitation_probability = 0;
    WeatherCondition condition = WeatherCondition::Unknown;
};

struct WeatherForecastData {
    CurrentWeather current{};
    uint8_t hourly_count = 0;
    HourlyWeather hourly[kWeatherHourlyCount]{};
    uint8_t daily_count = 0;
    DailyWeather daily[kWeatherDailyCount]{};
};
