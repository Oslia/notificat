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
#include "http_priv.hpp"
#include "http.hpp"

static const char *TAG = "HTTPS_CLIENT";

void HttpsTaskInit() {
    https_queue = xQueueCreate(10, sizeof(HttpsRequest*));
    xTaskCreate(HttpsTask, "HttpsTask", 5 * 1024, NULL, 1, &https_task_handle);

}


void HttpsTask(void *pvParameters) {
    while (1) {
        HttpsRequest *req;
        BaseType_t ret = xQueueReceive(https_queue, &req, 0);
        if (ret == pdTRUE) {
            if (req == NULL) {
                ESP_LOGE(TAG, "Received NULL request");
                continue;
            }
            ESP_LOGI(TAG, "Received HTTPS request: server=%s, url=%s, path=%s", req->server, req->url, req->path);
            // Process the request
            req->Process();
            delete req;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void HttpsTaskDeinit() {
    vQueueDelete(https_queue);
    // wait task idle
    vTaskDelete(https_task_handle);
}


HttpsClient::HttpsClient() {

}


HttpsClient::~HttpsClient() {

}


int HttpsClient::Request(HttpMethod method, const char* server, const char *url, const char *path, const char* body, void *context, void (*HttpsCallback)(void* context, char* data, size_t len, enum HttpsClientCode code)) {
    HttpsRequest* req = new HttpsRequest;
    req->method = method;
    strncpy(req->server, server, sizeof(req->server) - 1);
    strncpy(req->url, url, sizeof(req->url) - 1);
    strncpy(req->path, path, sizeof(req->path) - 1);
    if (body) {
        strncpy(req->body, body, sizeof(req->body) - 1);
    } else {
        req->body[0] = '\0';
    }
    req->context = context;
    req->callback = HttpsCallback;
    if (xQueueSend(https_queue, &req, 0) != pdTRUE)
    {
        delete req;
        return -1;
    }

    return 0;
}


void HttpsRequest::Process() {
    char data[1024];
    char chunk_size[32];
    int rcv_size;
    char *p;
    int ret, len;
    size_t written_bytes = 0;
    int body_length = 0;
    bool chunked = false;
    int content_length = -1;
    enum HttpsClientCode code = HTTPS_CLIENT_CODE_ERROR;
    
    esp_tls_t *tls = esp_tls_init();
    esp_tls_cfg_t cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

	std::string request;
    if (HttpMethod::GET == method) {
        request.append("GET ");
    } else if (HttpMethod::POST == method) {
        request.append("POST ");
    } else if (HttpMethod::PUT == method) {
        request.append("PUT ");
    } else if (HttpMethod::PATCH == method) {
        request.append("PATCH ");
    } else if (HttpMethod::DELETE == method) {
        request.append("DELETE ");
    }
    request.append(path);
    request.append(" HTTP/1.1\r\n");
    request.append("Host: ");
    request.append(server);
    request.append("\r\n");
    request.append("User-Agent: esp-idf/1.0 esp32\r\n");
    request.append("Connection: close\r\n");
    request.append("Content-Length: ");
    if (HttpMethod::POST == method || HttpMethod::PUT == method || HttpMethod::PATCH == method) {
        request.append(std::to_string(strlen(body)));
        request.append("\r\n");
        request.append("Content-Type: application/json\r\n");
        request.append("\r\n");
        request.append(body);
    } else {
        request.append("0");
        request.append("\r\n");
        request.append("\r\n");
    }

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

    if (strcasestr(data, "Transfer-Encoding: chunked"))
    {
        chunked = true;
    }

    p = strcasestr(data, "Content-Length:");

    if (p)
    {
        content_length = atoi(p + strlen("Content-Length:"));
    }

    p = strstr(data, "\r\n\r\n");
    if (p) {
        p = p + 4;
        
        if (chunked)
        {
            int i = 0;
            do {
                chunk_size[i] = p[i];
                i++;
                if (i > 30) break;
            } while('\r' != p[i]);
            chunk_size[i] = '\0';
            body_length = strtol(chunk_size, NULL, 16); //
            ESP_LOGI(TAG, "body length:%d", body_length);
            if (body_length < sizeof(data) - 1) {
                memmove(data, p + i + 2, body_length);
                data[body_length] = '\0';
            }
        }
        else if (content_length >= 0)
        {
            body_length = content_length;

            if (body_length < sizeof(data)-1)
            {
                memmove(
                    data,
                    p,
                    body_length
                );

                data[body_length] = '\0';
            }
        }
        
        code = HTTPS_CLIENT_CODE_OK;
    }

cleanup:
    if (tls)
    {
        esp_tls_conn_destroy(tls);
    }
    ESP_LOGI(TAG, "connection destroyed");

    if (callback) {
        callback(context, data, body_length, code);
    }
}

