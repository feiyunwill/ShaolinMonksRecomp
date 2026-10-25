#pragma once
#include <array>
#include <cstdint>

namespace openshao::ed_nexus {
class Sequence {
public:
    static constexpr unsigned kIdleLimit = 600;
    bool Feed(uint16_t pressed, bool main_menu, bool modifier_held) {
        if (!main_menu || !modifier_held) { ResetInput(); return false; }
        if (unlocked_) return false;
        if (!pressed) {
            if (position_ && ++idle_ > kIdleLimit) ResetInput();
            return false;
        }
        idle_ = 0;
        while (position_ && pressed != code_[position_]) position_ = prefix_[position_ - 1];
        if (pressed == code_[position_]) ++position_;
        if (position_ != code_.size()) return false;
        unlocked_ = true;
        ResetInput();
        return true;
    }
    bool Unlocked() const { return unlocked_; }
    void ResetInput() { position_ = idle_ = 0; }
private:
    // PxInput masks, after the game's Xbox-to-engine button conversion.
    static constexpr std::array<uint16_t,10> code_ = {0x1000,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000,0x80,0x20};
    static constexpr std::array<unsigned,10> prefix_ = {0,1,2,3,0,0,0,0,0,0};
    unsigned position_ = 0, idle_ = 0;
    bool unlocked_ = false;
};
inline bool OpenShortcut(bool unlocked, bool in_pause, bool in_nexus, uint16_t pressed) {
    return unlocked && in_pause && !in_nexus && pressed == 0x80;
}
}
