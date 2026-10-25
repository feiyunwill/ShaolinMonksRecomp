#pragma once
#include <array>
#include <string>
#include <cstdint>
#include "input_bindings.h"
namespace input { struct Pad; }

namespace openshao::mouse {
inline constexpr auto actions=[] { std::array<const char*,25> a{};a[0]="none";
    // Retain the original mouse action ordering for existing launcher selections.
    constexpr const char* first[]={"a","b","x","y","black","white","left_trigger","right_trigger","start","back","left_stick","right_stick"};
    for(unsigned i=0;i<12;++i)a[i+1]=first[i];
    for(unsigned i=10;i<14;++i)a[i+3]=bindings::keys[i];
    for(unsigned i=16;i<24;++i)a[i+1]=bindings::keys[i];return a; }();
inline constexpr std::array<const char*,7> button_keys{"left","right","middle","side1","side2","wheel_up","wheel_down"};
struct Settings {
    int sensitivity = 100;
    bool invert_y = false, invert_x = false;
    std::string motion = "right_stick";
    std::array<std::string,7> bindings{"x","b","y","black","white","none","none"};
    bool operator==(const Settings&) const = default;
};
class State {
public:
    void Configure(const Settings&);
    void Clear();
    void Move(int dx, int dy, double now);
    void Button(unsigned index, bool down);
    void Wheel(int direction,double now);
    input::Pad Sample(double now);
private:
    Settings settings;
    std::array<bool, 5> held{};
    std::array<double,2> wheel_until{};
    int64_t dx = 0, dy = 0;
    double sampled = -1, moved = -1;
    int16_t rx = 0, ry = 0;
};
}
