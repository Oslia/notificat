#include "lvgl.h"
#include "weather.hpp"
#include "weather_priv.hpp"
#include "esp_http_client.h"
#include "esp_log.h"

#define TAG "weather"

LV_IMG_DECLARE(weather_img);

namespace Weather {
	constexpr struct GCS city[Location::LOCATION_NUM] = {
		{35.6895, 139.6917},		// TOKYO
	};

// ====================
// Weather
// ====================
	Weather::Weather() {
		screen = lv_obj_create(NULL);
		time = lv_label_create(screen);
		icon = &weather_img;
		name = "weather";
	}


	void Weather::OnStart() {
		
	}


	void Weather::OnStop() {
		
	}


	lv_obj_t* Weather::Run() {
		return nullptr;
	}


// ====================
// WeatherPriv
// ====================


	WeatherPriv::WeatherPriv() {

	}


	WeatherPriv::~WeatherPriv() {

	}


	WeatherCode WeatherPriv::GetWeather() {
		std::string query = OPEN_METEO_ENDPOINT;

		query.append("latitued=%d", city[Location::TOKYO].latitude);
		query.append("&longitude=%d", city[Location::TOKYO].longitude);
		query.append("&daily=weather_code");
		query.append("&timezone=Asia/Tokyo");	//Asia%2FTokyo

		esp_http_client_config_t config = {
			.url = query.c_str(),
		};
		
		esp_http_client_handle_t client = esp_http_client_init(&config);
		
		// 요청 전송
		esp_err_t err = esp_http_client_perform(client);
		
		if (err == ESP_OK) {
			//ESP_LOGI(TAG, "HTTP Status = %d, Response = %s", esp_http_client_get_status_code(client), esp_http_client_get_response_body(client));
		} else {
			ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
		}

		// 클라이언트 정리
		esp_http_client_cleanup(client);

		return 0;
	}
}