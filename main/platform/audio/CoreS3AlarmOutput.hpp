#pragma once

#include <atomic>

#include "features/alarm/AlarmOutput.hpp"

class CoreS3AlarmOutput final : public AlarmOutput {
public:
    static CoreS3AlarmOutput &instance();

    void set_active(bool active) override;

private:
    CoreS3AlarmOutput() = default;

    static void task_entry(void *context);
    void run();
    bool open_speaker();
    bool play_alarm_file();
    void play_fallback_tone();

    std::atomic<bool> active_{false};
    void *task_ = nullptr;
    void *speaker_ = nullptr;
    bool speaker_open_ = false;
};
