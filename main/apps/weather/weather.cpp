#include <format>
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
	constexpr struct GCS city[Location::LOCATION_NUM] = {
		{35.6895, 139.6917},		// TOKYO
	};

// ====================
// Weather
// ====================
	Weather::Weather() {
		icon = &weather_img;
		name = "weather";
		weather = new WeatherPriv;

		screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

		container_weather_now = lv_image_create(screen);
		weather->UpdateWeather();
		const lv_image_dsc_t* icon = weather->GetWeatherIcon(weather->weather_code_for_day[0]);
		lv_image_set_src(container_weather_now, icon);
		lv_obj_align(container_weather_now, LV_ALIGN_CENTER, 0, 0);
		lv_image_set_scale(container_weather_now, 512);
	}

	
	Weather::~Weather() {
		lv_obj_delete(screen);
		lv_obj_delete(container_weather_now);
		delete weather;
	}

	
	void Weather::OnStart() {

	}


	void Weather::OnStop() {
		
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

		std::string path;
		path += "/v1/forecast?";
		path += std::format("latitude={}", city[Location::TOKYO].latitude);
		path += std::format("&longitude={}", city[Location::TOKYO].longitude);
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
				printf("  [%d] = %d\n", i, code);
				if (i < 7) {
					weather_code_for_day[i] = static_cast<WeatherCode>(code);
				}
			} else {
				printf("  [%d] = error\n", i);
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
}