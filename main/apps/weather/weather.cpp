#include <format>
#include "esp_timer.h"
#include "lvgl.h"
#include "weather.hpp"
#include "weather_priv.hpp"
#include "json_parser.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "http.hpp"

#define TAG "weather"

LV_IMG_DECLARE(weather_img);
LV_IMG_DECLARE(img_sunny);
LV_IMG_DECLARE(img_partly_cloudy);
LV_IMG_DECLARE(img_cloudy);
LV_IMG_DECLARE(img_foggy);
LV_IMG_DECLARE(img_drizzle);
LV_IMG_DECLARE(img_rain);
LV_IMG_DECLARE(img_snow);
LV_IMG_DECLARE(img_thunderstorm);

namespace Weather {
// ====================
// Weather
// ====================
	Weather::Weather() {
		icon = &weather_img;
		name = "weather";
		weather = new WeatherPriv;
		weather->UpdateWeather();
		weather->last_update_time = esp_timer_get_time();
	}

	
	Weather::~Weather() {
		if (nullptr != screen) {
			lv_obj_delete(screen);
		}

		delete weather;
	}

	
	void Weather::OnStart() {
		screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
		container_weather_now = lv_image_create(screen);
		const lv_image_dsc_t* icon = weather->GetWeatherIcon(weather->weather_code_for_day[0]);
		lv_image_set_src(container_weather_now, icon);
		lv_obj_align(container_weather_now, LV_ALIGN_CENTER, 0, 0);
		lv_image_set_scale(container_weather_now, 512);
	}


	void Weather::OnStop() {
		lv_obj_delete(screen);
		screen = nullptr;
	}


	void Weather::Run() {
		return;
	}


// ====================
// WeatherPriv
// ====================
	WeatherPriv::WeatherPriv() {
		
	}


	WeatherPriv::~WeatherPriv() {

	}


	void WeatherPriv::UpdateWeather() {
		HttpsClient http_client;
		char* buf = new char[1024];
		if (nullptr == buf) {
			return;
		}
		
		LoadConfig();

		std::string path;
		path += "/v1/forecast?";
		path += std::format("latitude={}", location[config.city].latitude);
		path += std::format("&longitude={}", location[config.city].longitude);
		path += "&daily=weather_code,temperature_2m_max,temperature_2m_min";
		path += "&timezone=Asia/Tokyo";

		std::string url;
		url.append(OPEN_METEO_URL);
		url.append(path);

		ESP_LOGI(TAG, "url:%s", url.c_str());
		ESP_LOGI(TAG, "path:%s", path.c_str());

    	http_client.GetRequest(WEB_SERVER, url.c_str(), path.c_str(), buf, 1024);
		ESP_LOGI(TAG, "data:%s", buf);

		jparse_ctx_t jparse_ctx;
		json_parse_start(&jparse_ctx, buf, strlen(buf));
		
		char json_buf[256];
		json_obj_get_object(&jparse_ctx, "daily");
		int num_codes = 0;
		if (json_obj_get_array(&jparse_ctx, "weather_code", &num_codes) != OS_SUCCESS) {
			return;
		}
		
		for (int i = 0; i < num_codes; i++) {
			int code = 0;
			if (json_arr_get_int(&jparse_ctx, i, &code) == OS_SUCCESS) {
				ESP_LOGI(TAG, "  [%d] = %d\n", i, code);
				if (i < 7) {
					weather_code_for_day[i] = static_cast<WeatherCode>(code);
				}
			} else {
				ESP_LOGI(TAG, "  [%d] = error\n", i);
			}
		}

		json_parse_end(&jparse_ctx);
		delete[] buf;
		
		return;
	}


	const lv_image_dsc_t* WeatherPriv::GetWeatherIcon(WeatherCode code) {
		if (90 <= code) {
			return &img_thunderstorm;
		}

		if (80 <= code) {
			return &img_drizzle;
		}

		if (70 <= code) {
			return &img_snow;
		}
		
		if (60 <= code) {
			return &img_rain;
		}

		if (50 <= code) {
			return &img_drizzle;
		}

		if (40 <= code) {
			return &img_foggy;
		}

		if (3 == code) {
			return &img_cloudy;
		}

		if (2 == code) {
			return &img_partly_cloudy;
		}

		if (1 == code) {
			return &img_sunny;
		}

		if (0 == code) {
			return &img_sunny;
		}
		
		return &img_sunny;		// Unknown code
	}


	void WeatherPriv::SetLocation(enum City city) {
		config.city = city;
		SaveConfig();
	}

	
	void WeatherPriv::SaveConfig() {
		FILE* f;
		f = fopen(WEATHER_CONFIG_FILE_PATH WEATHER_CONFIG_FILE, "w");
		if (NULL == f) {
			ESP_LOGI(TAG, "fail to save weather config\n");
			return;
		}
		//[TODO] .ini
		fprintf(f, "%d", config.city);
		fclose(f);
	}

	
	void WeatherPriv::LoadConfig() {
		FILE* f;
		f = fopen(WEATHER_CONFIG_FILE_PATH WEATHER_CONFIG_FILE, "r");
		if (NULL == f) {
			config.city = DEFAULT_CITY;		// default
			SaveConfig();
			ESP_LOGI(TAG, "weather fail to read file");
		} else {
			//[TODO] .ini
			int temp;
			fscanf(f, "%d", &temp);
			fclose(f);
			config.city = static_cast<City>(temp);
			ESP_LOGI(TAG, "weater config:%d", temp);
		}
	}
}

