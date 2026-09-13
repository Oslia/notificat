#pragma once

class AlarmOutput {
public:
    virtual ~AlarmOutput() = default;
    virtual void set_active(bool active) = 0;
};
