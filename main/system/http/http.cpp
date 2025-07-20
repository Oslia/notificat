/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"
#include "http.hpp"

static const char *TAG = "HTTPS_CLIENT";

HttpsClient::HttpsClient() {

}


HttpsClient::~HttpsClient() {

}


void HttpsClient::GetRequest(const char* server, const char *url, const char *path, char* buf, size_t buf_size) {
    char data[1024];
    char chunk_size[32];
    int rcv_size;
    char *p;
    int ret, len;
    size_t written_bytes = 0;
    esp_tls_t *tls = esp_tls_init();
    esp_tls_cfg_t cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    if (NULL == buf) {
        ESP_LOGE(TAG, "null buf!");
    }
    
	std::string request;
    request.append("GET ");
    request.append(path);
    request.append(" HTTP/1.1\r\n");
    request.append("Host: ");
    request.append(server);
    request.append("\r\n");
    request.append("User-Agent: esp-idf/1.0 esp32\r\n");
    request.append("\r\n");

    ESP_LOGI(TAG, "req:%s", request.c_str());

    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        goto cleanup;
    }

    if (esp_tls_conn_http_new_sync(url, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        int esp_tls_code = 0, esp_tls_flags = 0;
        esp_tls_error_handle_t tls_e = NULL;
        esp_tls_get_error_handle(tls, &tls_e);
        /* Try to get TLS stack level error and certificate failure flags, if any */
        ret = esp_tls_get_and_clear_last_error(tls_e, &esp_tls_code, &esp_tls_flags);
        if (ret == ESP_OK) {
            ESP_LOGE(TAG, "TLS error = -0x%x, TLS flags = -0x%x", esp_tls_code, esp_tls_flags);
        }
        goto cleanup;
    }

    do {
        ret = esp_tls_conn_write(tls,
                                 request.c_str() + written_bytes,
                                 request.size() - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < request.size());

    ESP_LOGI(TAG, "Reading HTTP response...");
    memset((void *)data, 0x00, sizeof(data));
    rcv_size = 0;
    len = sizeof(data) - 1;
    do {
        ret = esp_tls_conn_read(tls, (char *)(data + rcv_size), len - rcv_size);

        ESP_LOGI(TAG, "test");
        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }
        rcv_size += ret;
        ESP_LOGD(TAG, "%d bytes read", len);
//        /* Print response directly to stdout as it is read */
//        for (int i = 0; i < rcv_size; i++) {
//            putchar(data[i]);
//        }
//        putchar('\n'); // JSON output doesn't have a newline at end
    } while (1);

    p = strstr(data, "\r\n\r\n");
    if (p) {
        p = p + 4;
        int i = 0;
        do {
            chunk_size[i] = p[i];
            i++;
            if (i > 30) break;
        } while('\r' != p[i]);
        chunk_size[i] = '\0';
        int body_length = strtol(chunk_size, NULL, 16); //
        ESP_LOGI(TAG, "body length:%d", body_length);
        if (body_length < buf_size - 1) {
            memcpy(buf, p + i + 2, body_length);
            buf[body_length] = '\0';
        }
    }

cleanup:
    esp_tls_conn_destroy(tls);
    ESP_LOGI(TAG, "connection destroyed");
}

