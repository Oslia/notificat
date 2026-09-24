#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mqtt_detail {
constexpr size_t TopicLimit = 256;
constexpr size_t PayloadLimit = 1024;
constexpr size_t ListenerLimit = 16;

// MQTT UTF-8 と AWS の一般トピック制約を検証する。ワイルドカードは未対応。
inline bool valid_topic(const char *topic)
{
    if (!topic) return false;
    const size_t length = strnlen(topic, TopicLimit + 1);
    if (length == 0 || length > TopicLimit) return false;
    unsigned slashes = 0;
    for (size_t i = 0; i < length;) {
        const auto c = static_cast<uint8_t>(topic[i++]);
        if (c == '+' || c == '#') return false;
        if (c == '/' && ++slashes > 7) return false;
        if (c < 0x80) continue;
        unsigned remaining;
        uint32_t code, minimum;
        if (c >= 0xc2 && c <= 0xdf) { remaining = 1; code = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { remaining = 2; code = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { remaining = 3; code = c & 7; minimum = 0x10000; }
        else return false;
        while (remaining--) {
            if (i == length) return false;
            const auto next = static_cast<uint8_t>(topic[i++]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}

struct Frame {
    char topic[TopicLimit + 1]{};
    uint8_t payload[PayloadLimit]{};
    size_t size = 0;
    std::array<uint32_t, ListenerLimit> recipients{};
};

class Assembly {
public:
    Frame frame;
    void reset() { active_ = false; received_ = 0; }
    // 最初の断片で呼び出し元が recipients を確定する。
    bool append(const char *topic, size_t topic_size, const void *data,
                size_t size, size_t offset, size_t total)
    {
        if (offset == 0) {
            reset();
            if (!topic || topic_size == 0 || topic_size > TopicLimit || total > PayloadLimit ||
                std::memchr(topic, 0, topic_size)) return false;
            std::memcpy(frame.topic, topic, topic_size);
            frame.topic[topic_size] = 0;
            if (!valid_topic(frame.topic)) return false;
            frame.size = total;
            active_ = true;
        }
        if (!active_ || total != frame.size || offset != received_ ||
            offset > total || size > total - offset || (size && !data)) {
            reset();
            return false;
        }
        if (size) std::memcpy(frame.payload + offset, data, size);
        received_ += size;
        if (received_ != total) return false;
        active_ = false;
        return true;
    }
private:
    bool active_ = false;
    size_t received_ = 0;
};
} // namespace mqtt_detail
