#include "core/SystemBootstrap.h"

#include "core/SystemManager.hpp"

extern "C" esp_err_t system_bootstrap_init(void)
{
    return SystemManager::instance().init();
}
