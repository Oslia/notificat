#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "http.hpp"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define WEB_URL "https://api.open-meteo.com/v1/forecast?latitude=35.6895&longitude=139.6917&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=Asia/Tokyo"
#define WEB_SERVER "api.open-meteo.com"
static const char req[] = "GET " WEB_URL " HTTP/1.1\r\n"
                             "Host: "WEB_SERVER"\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";

static const char* TAG = "mytest";

void mytest(void) {
	
	esp_tls_cfg_t cfg = {
		.crt_bundle_attach = esp_crt_bundle_attach,
    };
    HttpClient http_client;
    http_client.GetRequest(cfg, WEB_URL, req);
}
