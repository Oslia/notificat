#include "features/weather/providers/OpenMeteoProvider.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"

namespace {

constexpr size_t kResponseCapacity = 16 * 1024;
constexpr char kForecastUrl[] =
    "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
    "&hourly=temperature_2m,weather_code,precipitation_probability"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&forecast_hours=24&forecast_days=7&timeformat=unixtime&timezone=auto";

struct ResponseBuffer {
    char *data = nullptr;
    size_t size = 0;
    size_t capacity = 0;
    bool overflow = false;
};

esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
        return ESP_OK;
    }

    auto *buffer = static_cast<ResponseBuffer *>(event->user_data);
    const size_t incoming = static_cast<size_t>(event->data_len);
    /* 末尾の NUL 用に 1 バイト残し、想定外に大きい応答は切り詰めず失敗させる。 */
    if (buffer == nullptr || buffer->size + incoming >= buffer->capacity) {
        if (buffer != nullptr) {
            buffer->overflow = true;
        }
        return ESP_FAIL;
    }

    std::memcpy(buffer->data + buffer->size, event->data, incoming);
    buffer->size += incoming;
    buffer->data[buffer->size] = '\0';
    return ESP_OK;
}

WeatherCondition condition_from_wmo(int code)
{
    if (code == 0) {
        return WeatherCondition::Clear;
    }
    if (code == 1 || code == 2) {
        return WeatherCondition::PartlyCloudy;
    }
    if (code == 3) {
        return WeatherCondition::Overcast;
    }
    if (code == 45 || code == 48) {
        return WeatherCondition::Fog;
    }
    if (code >= 51 && code <= 57) {
        return WeatherCondition::Drizzle;
    }
    if ((code >= 61 && code <= 67)) {
        return WeatherCondition::Rain;
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return WeatherCondition::Snow;
    }
    if (code >= 80 && code <= 82) {
        return WeatherCondition::Shower;
    }
    if (code >= 95) {
        return WeatherCondition::Thunderstorm;
    }
    return WeatherCondition::Unknown;
}

cJSON *object_item(cJSON *object, const char *name)
{
    return object != nullptr ? cJSON_GetObjectItemCaseSensitive(object, name) : nullptr;
}

bool number_value(cJSON *object, const char *name, double &value)
{
    cJSON *item = object_item(object, name);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    value = item->valuedouble;
    return true;
}

bool array_number(cJSON *array, int index, double &value)
{
    cJSON *item = cJSON_GetArrayItem(array, index);
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    value = item->valuedouble;
    return true;
}

uint8_t percentage(double value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0, 100.0));
}

bool parse_current(cJSON *root, WeatherForecastData &forecast)
{
    cJSON *current = object_item(root, "current");
    double time = 0;
    double temperature = 0;
    double apparent = 0;
    double humidity = 0;
    double weather_code = 0;
    double wind_speed = 0;
    if (!number_value(current, "time", time)
        || !number_value(current, "temperature_2m", temperature)
        || !number_value(current, "apparent_temperature", apparent)
        || !number_value(current, "relative_humidity_2m", humidity)
        || !number_value(current, "weather_code", weather_code)
        || !number_value(current, "wind_speed_10m", wind_speed)) {
        return false;
    }

    forecast.current.valid = true;
    forecast.current.time = static_cast<int64_t>(time);
    forecast.current.temperature_c = static_cast<float>(temperature);
    forecast.current.apparent_temperature_c = static_cast<float>(apparent);
    forecast.current.relative_humidity = percentage(humidity);
    forecast.current.condition = condition_from_wmo(static_cast<int>(weather_code));
    forecast.current.wind_speed_kmh = static_cast<float>(wind_speed);
    return true;
}

bool parse_hourly(cJSON *root, WeatherForecastData &forecast)
{
    cJSON *hourly = object_item(root, "hourly");
    cJSON *times = object_item(hourly, "time");
    cJSON *temperatures = object_item(hourly, "temperature_2m");
    cJSON *weather_codes = object_item(hourly, "weather_code");
    cJSON *precipitation = object_item(hourly, "precipitation_probability");
    if (!cJSON_IsArray(times) || !cJSON_IsArray(temperatures)
        || !cJSON_IsArray(weather_codes) || !cJSON_IsArray(precipitation)) {
        return false;
    }

    const int available = std::min({cJSON_GetArraySize(times), cJSON_GetArraySize(temperatures),
                                    cJSON_GetArraySize(weather_codes), cJSON_GetArraySize(precipitation)});
    /* API の 1 時間データから 3 時間ごとの表示点だけを抽出する。 */
    for (int source = 0; source < available && forecast.hourly_count < kWeatherHourlyCount; source += 3) {
        double time = 0;
        double temperature = 0;
        double code = 0;
        double rain_probability = 0;
        if (!array_number(times, source, time) || !array_number(temperatures, source, temperature)
            || !array_number(weather_codes, source, code)
            || !array_number(precipitation, source, rain_probability)) {
            return false;
        }

        HourlyWeather &item = forecast.hourly[forecast.hourly_count++];
        item.time = static_cast<int64_t>(time);
        item.temperature_c = static_cast<float>(temperature);
        item.condition = condition_from_wmo(static_cast<int>(code));
        item.precipitation_probability = percentage(rain_probability);
    }
    return forecast.hourly_count > 0;
}

bool parse_daily(cJSON *root, WeatherForecastData &forecast)
{
    cJSON *daily = object_item(root, "daily");
    cJSON *times = object_item(daily, "time");
    cJSON *minimums = object_item(daily, "temperature_2m_min");
    cJSON *maximums = object_item(daily, "temperature_2m_max");
    cJSON *weather_codes = object_item(daily, "weather_code");
    cJSON *precipitation = object_item(daily, "precipitation_probability_max");
    cJSON *offset_item = object_item(root, "utc_offset_seconds");
    if (!cJSON_IsArray(times) || !cJSON_IsArray(minimums) || !cJSON_IsArray(maximums)
        || !cJSON_IsArray(weather_codes) || !cJSON_IsArray(precipitation)
        || !cJSON_IsNumber(offset_item)) {
        return false;
    }

    const int available = std::min({cJSON_GetArraySize(times), cJSON_GetArraySize(minimums),
                                    cJSON_GetArraySize(maximums), cJSON_GetArraySize(weather_codes),
                                    cJSON_GetArraySize(precipitation), static_cast<int>(kWeatherDailyCount)});
    const int64_t offset = static_cast<int64_t>(offset_item->valuedouble);
    for (int index = 0; index < available; ++index) {
        double time = 0;
        double minimum = 0;
        double maximum = 0;
        double code = 0;
        double rain_probability = 0;
        if (!array_number(times, index, time) || !array_number(minimums, index, minimum)
            || !array_number(maximums, index, maximum) || !array_number(weather_codes, index, code)
            || !array_number(precipitation, index, rain_probability)) {
            return false;
        }

        DailyWeather &item = forecast.daily[forecast.daily_count++];
        /* WeatherApp 側で gmtime_r() を使っても地域の日付になるよう補正する。 */
        item.local_date = static_cast<int64_t>(time) + offset;
        item.minimum_temperature_c = static_cast<float>(minimum);
        item.maximum_temperature_c = static_cast<float>(maximum);
        item.condition = condition_from_wmo(static_cast<int>(code));
        item.precipitation_probability = percentage(rain_probability);
    }
    return forecast.daily_count == kWeatherDailyCount;
}

esp_err_t parse_forecast(const char *json, size_t length, WeatherForecastData &forecast)
{
    cJSON *root = cJSON_ParseWithLength(json, length);
    if (root == nullptr) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    forecast = WeatherForecastData{};
    const bool valid = parse_current(root, forecast)
                       && parse_hourly(root, forecast)
                       && parse_daily(root, forecast);
    cJSON_Delete(root);
    return valid ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

} // namespace

OpenMeteoProvider &OpenMeteoProvider::instance()
{
    static OpenMeteoProvider provider;
    return provider;
}

esp_err_t OpenMeteoProvider::fetch(const WeatherLocation &location, WeatherForecastData &forecast)
{
    char url[640];
    const int url_length = std::snprintf(url, sizeof(url), kForecastUrl,
                                         location.latitude, location.longitude);
    if (url_length < 0 || static_cast<size_t>(url_length) >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    ResponseBuffer response{};
    /* LCD の DMA バッファ用内部 SRAM を圧迫しないよう、応答は PSRAM を優先する。 */
    response.data = static_cast<char *>(heap_caps_malloc(kResponseCapacity,
                                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (response.data == nullptr) {
        response.data = static_cast<char *>(heap_caps_malloc(kResponseCapacity, MALLOC_CAP_8BIT));
    }
    if (response.data == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    response.capacity = kResponseCapacity;
    response.data[0] = '\0';

    esp_http_client_config_t config{};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = &response;
    config.timeout_ms = 12000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        heap_caps_free(response.data);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "Notificat/0.1");

    esp_err_t result = esp_http_client_perform(client);
    if (result == ESP_OK && esp_http_client_get_status_code(client) != 200) {
        result = ESP_FAIL;
    }
    if (result == ESP_OK && response.overflow) {
        result = ESP_ERR_INVALID_SIZE;
    }
    if (result == ESP_OK) {
        result = parse_forecast(response.data, response.size, forecast);
    }

    esp_http_client_cleanup(client);
    heap_caps_free(response.data);
    return result;
}
