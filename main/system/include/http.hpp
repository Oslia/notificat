#ifndef HTTP_HPP_
#define HTTP_HPP_

#include "esp_tls.h"

enum HttpsClientState {
	HTTPS_CLIENT_STATE_CONNECT = 0,
	HTTPS_CLIENT_STATE_SEND,
	HTTPS_CLIENT_STATE_RECEIVE,
	HTTPS_CLIENT_STATE_PARSE,
	HTTPS_CLIENT_STATE_DONE,
	HTTPS_CLIENT_STATE_DESTROY,
};

enum HttpsClientCode {
	HTTPS_CLIENT_CODE_OK = 0,
	HTTPS_CLIENT_CODE_ERROR,
};

enum class HttpMethod
{
    GET,
    POST,
    PUT,
    PATCH,
    DELETE
};

class HttpsClient {
public:
	void Process();
	HttpsClient();
	~HttpsClient();
	int Request(HttpMethod method, const char* server, const char *url, const char *path, const char* body, void *context, void (*HttpsCallback)(void* context, char* data, size_t len, enum HttpsClientCode code));
};

void HttpsTaskInit();
void HttpsTaskDeinit();

#endif	// HTTP_HPP_
