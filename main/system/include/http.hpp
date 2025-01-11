#ifndef HTTP_HPP_
#define HTTP_HPP_

#include "esp_tls.h"

class HttpClient {
public:
	HttpClient();
	~HttpClient();
	void GetRequest(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST);
};

#endif	// HTTP_HPP_