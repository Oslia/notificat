#pragma once

#include <cstddef>
#include <cstdint>

class App;

enum class AppId : uint8_t {
    Home,
    Alarm,
    Weather,
    DeviceSettings,
};

struct AppDescriptor {
    AppId id;
    const char *title;
    App *app;
};

class AppRegistry {
public:
    static constexpr size_t kMaxApps = 8;

    bool add(AppId id, const char *title, App &app);
    App *find(AppId id) const;
    size_t count() const;
    const AppDescriptor &at(size_t index) const;

private:
    AppDescriptor entries_[kMaxApps]{};
    size_t count_ = 0;
};
