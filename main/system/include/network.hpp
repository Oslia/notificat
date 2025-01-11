#ifndef SYSTEM_NETWORK_H_
#define SYSTEM_NETWORK_H_

#include "singleton.hpp"
#include "esp_event.h"

class Network: public Singleton<Network> {
	friend class Singleton;

public:
	void ConnectWifi(const char* ssid, const char* pw);

private:
	Network();
	~Network();
	static void EventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

};


#endif	// SYSTEM_NETWORK_H_