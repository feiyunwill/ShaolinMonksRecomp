#pragma once

namespace openshao {
class ShadowCaptureSequence {
public:
    void Start(bool suite) { step_ = suite ? 1 : 0; }
    bool Next() { step_ = step_ && step_ < 5 ? step_ + 1 : 0; return Active(); }
    unsigned Step() const { return step_; }
    bool Active() const { return step_ != 0; }
    bool Readback() const { return step_ == 2; }
    bool Raster(bool fallback) const { return Active() ? step_ == 3 || step_ == 5 : fallback; }
    bool Manual(bool fallback) const { return Active() ? step_ == 4 || step_ == 5 : fallback; }
    const char* Name() const {
        constexpr const char* names[] = {"single", "01-native", "02-readback", "03-raster", "04-manual", "05-combined"};
        return names[step_];
    }
private:
    unsigned step_ = 0;
};
}
