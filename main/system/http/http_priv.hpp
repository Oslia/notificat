#ifndef HTTP_PRIV_HPP_
#define HTTP_PRIV_HPP_

#include "freertos/task.h"
#include "http.hpp"

QueueHandle_t https_queue;
TaskHandle_t https_task_handle;

class HttpsRequest {
public:
    HttpMethod method;

	char server[128];
    char url[128];
    char path[128];

    char body[512];
    void* context;
    void (*callback)(
        void *context,
        char *data,
        size_t length,
        HttpsClientCode code
    );
	void Process();
};

void HttpsTask(void *pvParameters);

#endif	// HTTP_PRIV_HPP_
