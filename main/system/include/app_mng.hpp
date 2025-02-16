#ifndef APP_MNG_HPP_
#define APP_MNG_HPP_

#include <vector>
#include "freertos/FreeRTOS.h"
#include "singleton.hpp"
#include "app.hpp"

class AppMngPriv;

class AppMng: public Singleton<AppMng> {
public:
	AppMng();
	~AppMng();
	void Run();
	int RegisterApp(App* app);
private:
	AppMngPriv* impl;
	friend Singleton;
};


#endif	// APP_MNG_HPP_