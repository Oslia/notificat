#include "ui/app/AppRegistry.hpp"

#include "ui/app/App.hpp"

bool AppRegistry::add(AppId id, const char *title, App &app)
{
    if (count_ >= kMaxApps || find(id) != nullptr) {
        return false;
    }
    entries_[count_++] = {id, title, &app};
    return true;
}

App *AppRegistry::find(AppId id) const
{
    for (size_t index = 0; index < count_; ++index) {
        if (entries_[index].id == id) {
            return entries_[index].app;
        }
    }
    return nullptr;
}

size_t AppRegistry::count() const
{
    return count_;
}

const AppDescriptor &AppRegistry::at(size_t index) const
{
    return entries_[index < count_ ? index : 0];
}
