#pragma once

#include "core/SystemEvent.hpp"
#include "lvgl.h"

struct AppContext;

class App {
public:
    virtual ~App() = default;

    virtual void init(AppContext &context) { (void)context; }
    virtual void onEnter(lv_obj_t *parent) = 0;
    virtual void onLeave() {}
    virtual void onSystemEvents(SystemEventMask events) { (void)events; }

protected:
    static void addLabel(lv_obj_t *parent, const char *text);
};
