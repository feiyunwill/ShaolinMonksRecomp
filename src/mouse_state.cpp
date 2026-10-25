#include "mouse_state.h"
#include "core/input.h"
#include <algorithm>
#include <cmath>
namespace openshao::mouse {
void State::Configure(const Settings& value) { settings = value; Clear(); }
void State::Clear() { held = {}; wheel_until={}; dx = dy = 0; sampled = moved = -1; rx = ry = 0; }
void State::Move(int x, int y, double now) {
    dx = std::clamp(dx + x, int64_t(-1000000), int64_t(1000000));
    dy = std::clamp(dy + y, int64_t(-1000000), int64_t(1000000));
    moved = now;
    if (sampled < 0) sampled = now - 1.0 / 60;
}
void State::Button(unsigned index, bool down) { if (index < held.size()) held[index] = down; }
void State::Wheel(int direction,double now) { if(direction)wheel_until[direction>0?0:1]=now+.08; }
input::Pad State::Sample(double now) {
    if (sampled >= 0 && now - sampled >= 1.0 / 120 - 1e-9) {
        const auto seconds = now - sampled > .05 ? 1.0 / 60 : now - sampled;
        const auto axis = [&](int64_t value) {
            return int16_t(std::lround(std::clamp(value * 20.0 * settings.sensitivity / 100 / seconds, -32767.0, 32767.0)));
        };
        rx = axis(settings.invert_x ? -dx : dx); ry = axis(settings.invert_y ? dy : -dy);
        dx = dy = 0; sampled = now;
    }
    if (now - moved > .05) rx = ry = 0;
    input::Pad pad{};
    if(settings.motion=="right_stick") {pad.rx=rx;pad.ry=ry;}
    else if(settings.motion=="left_stick") {pad.lx=rx;pad.ly=ry;}
    bindings::Values values{};
    for(unsigned i=0;i<settings.bindings.size();++i) {
        const bool active=i<held.size()?held[i]:now<wheel_until[i-held.size()];
        if(!active)continue;
        const auto action=std::find(bindings::keys.begin(),bindings::keys.end(),settings.bindings[i]);
        if(action!=bindings::keys.end())values[action-bindings::keys.begin()]=1;
    }
    bindings::Apply(pad,values);
    return pad;
}
}
