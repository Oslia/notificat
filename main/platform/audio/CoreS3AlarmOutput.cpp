#include "platform/audio/CoreS3AlarmOutput.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kLogTag[] = "AlarmOutput";
constexpr char kAlarmSoundPath[] = "/spiflash/audio/alarm_chime.wav";
constexpr uint32_t kSampleRate = 22050;
constexpr int kOutputVolume = 80;
constexpr uint32_t kToneFrequency = 880;
constexpr size_t kSampleCount = 512;
constexpr int kToneChunks = 12;
constexpr int kSilenceChunks = 10;
constexpr uint32_t kOutputTaskStackSize = 6144;

struct WavInfo {
    long data_offset = 0;
    uint32_t data_size = 0;
};

uint16_t read_le16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0])
        | static_cast<uint16_t>(bytes[1] << 8);
}

uint32_t read_le32(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8)
        | (static_cast<uint32_t>(bytes[2]) << 16)
        | (static_cast<uint32_t>(bytes[3]) << 24);
}

bool read_wav_info(FILE *file, WavInfo &info)
{
    uint8_t riff_header[12]{};
    if (std::fread(riff_header, 1, sizeof(riff_header), file) != sizeof(riff_header)
        || std::memcmp(riff_header, "RIFF", 4) != 0
        || std::memcmp(riff_header + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool format_valid = false;
    bool data_found = false;
    uint8_t chunk_header[8]{};
    while (std::fread(chunk_header, 1, sizeof(chunk_header), file) == sizeof(chunk_header)) {
        const uint32_t chunk_size = read_le32(chunk_header + 4);
        const long chunk_offset = std::ftell(file);
        if (chunk_offset < 0) {
            return false;
        }

        if (std::memcmp(chunk_header, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                return false;
            }
            uint8_t format[16]{};
            if (std::fread(format, 1, sizeof(format), file) != sizeof(format)) {
                return false;
            }
            format_valid = read_le16(format) == 1
                && read_le16(format + 2) == 1
                && read_le32(format + 4) == kSampleRate
                && read_le16(format + 14) == 16;
        } else if (std::memcmp(chunk_header, "data", 4) == 0) {
            info.data_offset = chunk_offset;
            info.data_size = chunk_size;
            data_found = chunk_size > 0 && (chunk_size % sizeof(int16_t)) == 0;
        }

        if (format_valid && data_found) {
            return true;
        }

        const uint64_t next_offset = static_cast<uint64_t>(chunk_offset)
            + chunk_size + (chunk_size & 1U);
        if (next_offset > static_cast<uint64_t>(std::numeric_limits<long>::max())
            || std::fseek(file, static_cast<long>(next_offset), SEEK_SET) != 0) {
            return false;
        }
    }

    return false;
}

} // namespace

CoreS3AlarmOutput &CoreS3AlarmOutput::instance()
{
    static CoreS3AlarmOutput output;
    return output;
}

void CoreS3AlarmOutput::set_active(bool active)
{
    active_.store(active, std::memory_order_release);

    if (active && task_ == nullptr) {
        TaskHandle_t task_handle = nullptr;
        if (xTaskCreate(task_entry, "alarm_audio", kOutputTaskStackSize,
                        this, 4, &task_handle) != pdPASS) {
            ESP_LOGE(kLogTag, "Unable to create alarm audio task");
            return;
        }
        task_ = task_handle;
    }

    if (task_ != nullptr) {
        xTaskNotifyGive(static_cast<TaskHandle_t>(task_));
    }
}

void CoreS3AlarmOutput::task_entry(void *context)
{
    static_cast<CoreS3AlarmOutput *>(context)->run();
}

bool CoreS3AlarmOutput::open_speaker()
{
    if (speaker_open_) {
        return true;
    }
    auto speaker = static_cast<esp_codec_dev_handle_t>(speaker_);
    if (speaker == nullptr) {
        speaker = bsp_audio_codec_speaker_init();
        speaker_ = speaker;
    }
    if (speaker == nullptr) {
        ESP_LOGE(kLogTag, "Unable to initialize speaker");
        return false;
    }

    esp_codec_dev_sample_info_t format{};
    format.bits_per_sample = 16;
    format.channel = 1;
    format.sample_rate = kSampleRate;
    format.mclk_multiple = I2S_MCLK_MULTIPLE_384;

    const int volume_result = esp_codec_dev_set_out_vol(speaker, kOutputVolume);
    const int open_result = esp_codec_dev_open(speaker, &format);
    if (volume_result != ESP_CODEC_DEV_OK || open_result != ESP_CODEC_DEV_OK) {
        ESP_LOGE(kLogTag, "Unable to open speaker codec (volume=%d, open=%d)",
                 volume_result, open_result);
        return false;
    }

    esp_codec_dev_set_out_mute(speaker, false);
    speaker_open_ = true;
    ESP_LOGI(kLogTag, "Speaker ready: %lu Hz, mono, 16 bit, volume %d%%",
             static_cast<unsigned long>(kSampleRate), kOutputVolume);
    return true;
}

bool CoreS3AlarmOutput::play_alarm_file()
{
    FILE *file = std::fopen(kAlarmSoundPath, "rb");
    if (file == nullptr) {
        ESP_LOGW(kLogTag, "Alarm sound is unavailable: %s", kAlarmSoundPath);
        return false;
    }

    WavInfo wav{};
    if (!read_wav_info(file, wav)) {
        ESP_LOGE(kLogTag, "Unsupported or damaged alarm WAV: %s", kAlarmSoundPath);
        std::fclose(file);
        return false;
    }

    ESP_LOGI(kLogTag, "Playing alarm sound: %s", kAlarmSoundPath);
    auto speaker = static_cast<esp_codec_dev_handle_t>(speaker_);
    int16_t samples[kSampleCount]{};
    while (active_.load(std::memory_order_acquire)) {
        if (std::fseek(file, wav.data_offset, SEEK_SET) != 0) {
            std::fclose(file);
            return false;
        }

        uint32_t remaining = wav.data_size;
        while (remaining > 0 && active_.load(std::memory_order_acquire)) {
            const size_t requested = remaining < sizeof(samples) ? remaining : sizeof(samples);
            const size_t read = std::fread(samples, 1, requested, file);
            if (read == 0 || esp_codec_dev_write(speaker, samples, read) != ESP_CODEC_DEV_OK) {
                ESP_LOGE(kLogTag, "Alarm sound playback failed");
                std::fclose(file);
                return false;
            }
            remaining -= static_cast<uint32_t>(read);
        }

        std::memset(samples, 0, sizeof(samples));
        for (int chunk = 0; chunk < kSilenceChunks
                            && active_.load(std::memory_order_acquire); ++chunk) {
            if (esp_codec_dev_write(speaker, samples, sizeof(samples)) != ESP_CODEC_DEV_OK) {
                ESP_LOGE(kLogTag, "Alarm silence playback failed");
                std::fclose(file);
                return false;
            }
        }
    }

    std::fclose(file);
    return true;
}

void CoreS3AlarmOutput::play_fallback_tone()
{
    auto speaker = static_cast<esp_codec_dev_handle_t>(speaker_);
    int16_t samples[kSampleCount]{};
    uint32_t phase = 0;

    ESP_LOGW(kLogTag, "Using generated fallback alarm tone");
    while (active_.load(std::memory_order_acquire)) {
        for (int chunk = 0; chunk < kToneChunks
                            && active_.load(std::memory_order_acquire); ++chunk) {
            for (size_t index = 0; index < kSampleCount; ++index) {
                phase += kToneFrequency;
                if (phase >= kSampleRate) {
                    phase -= kSampleRate;
                }
                samples[index] = phase < kSampleRate / 2 ? 6500 : -6500;
            }
            esp_codec_dev_write(speaker, samples, sizeof(samples));
        }

        std::memset(samples, 0, sizeof(samples));
        for (int chunk = 0; chunk < kSilenceChunks
                            && active_.load(std::memory_order_acquire); ++chunk) {
            esp_codec_dev_write(speaker, samples, sizeof(samples));
        }
    }
}

void CoreS3AlarmOutput::run()
{
    int16_t silence[kSampleCount]{};

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!active_.load(std::memory_order_acquire)) {
            continue;
        }
        if (!open_speaker()) {
            continue;
        }

        auto speaker = static_cast<esp_codec_dev_handle_t>(speaker_);
        if (!play_alarm_file() && active_.load(std::memory_order_acquire)) {
            play_fallback_tone();
        }

        esp_codec_dev_write(speaker, silence, sizeof(silence));
    }
}
