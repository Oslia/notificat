#include "core/MqttRouting.hpp"
#include <cassert>
#include <string>

int main()
{
    using namespace mqtt_detail;
    assert(valid_topic("notificat/test"));
    assert(valid_topic("notificat/天気"));
    assert(valid_topic(std::string(256, 'a').c_str()));
    assert(!valid_topic(std::string(257, 'a').c_str()));
    for (const char *topic : {"", "a/+", "a/#", "a/b/c/d/e/f/g/h/i", "\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe3\x81"})
        assert(!valid_topic(topic));
    assert(!valid_topic(nullptr));
    Assembly a;
    a.frame.recipients[0] = 7;
    const char data[] = {'a', 0, 'b', 'c'};
    assert(!a.append("test", 4, data, 2, 0, 4));
    assert(a.append(nullptr, 0, data + 2, 2, 2, 4));
    assert(a.frame.size == 4 && std::memcmp(a.frame.payload, data, 4) == 0);
    assert(a.frame.recipients[0] == 7);
    assert(!a.append(nullptr, 0, data, 1, 4, 4));
    assert(a.append("empty", 5, nullptr, 0, 0, 0));
    assert(!a.append("test", 4, data, 2, 0, 4));
    assert(!a.append(nullptr, 0, data, 1, 3, 4));
    assert(!a.append(nullptr, 0, data, 2, 2, 4));
    assert(!a.append("test", 4, data, 2, 0, 4));
    a.reset();
    assert(!a.append(nullptr, 0, data, 2, 2, 4));
    assert(!a.append("a\0b", 3, data, 1, 0, 1));
    assert(!a.append("test", 4, nullptr, 1, 0, 1));
    std::string maximum(1024, 'x');
    assert(a.append("test", 4, maximum.data(), 1024, 0, 1024));
    assert(!a.append("test", 4, maximum.data(), 1024, 0, 1025));
    assert(!a.append("test", 4, data, 4, 0, 3));
}
