#ifndef HTTP_HPP_
#define HTTP_HPP_

#include "esp_tls.h"

class HttpsClient {
public:
	HttpsClient();
	~HttpsClient();
	void GetRequest(const char* server, const char *url, const char *path, char* buf, size_t buf_size);
};

#endif	// HTTP_HPP_