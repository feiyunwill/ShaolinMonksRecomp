#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "shadow_capture.h"

int main() {
    openshao::ShadowCaptureSequence sequence;
    if (sequence.Active() || sequence.Next() || sequence.Raster(false) || !sequence.Manual(true)) return 1;
    sequence.Start(true);
    const char* names[] = {"01-native", "02-readback", "03-raster", "04-manual", "05-combined"};
    const bool raster[] = {false, false, true, false, true};
    const bool manual[] = {false, false, false, true, true};
    for (unsigned i = 0; i < 5; ++i) {
        if (!sequence.Active() || sequence.Step() != i + 1 || std::strcmp(sequence.Name(), names[i])) {
            std::puts("FAIL: one capture request must advance through all five diagnostic modes"); return 1;
        }
        for (bool fallback : {false, true}) {
            if (sequence.Raster(fallback) != raster[i] || sequence.Manual(fallback) != manual[i] ||
                sequence.Readback() != (i == 1)) {
                std::puts("FAIL: diagnostic modes must isolate each change, independent of inherited flags"); return 1;
            }
        }
        if (sequence.Next() != (i < 4)) return 1;
    }
    if (sequence.Active() || sequence.Readback() || sequence.Raster(false) || !sequence.Raster(true) ||
        sequence.Manual(false) || !sequence.Manual(true) || sequence.Next()) {
        std::puts("FAIL: capture completion must restore ordinary rendering options"); return 1;
    }
    sequence.Start(true);
    sequence.Start(false);
    if (sequence.Active() || sequence.Next()) return 1;
    std::puts("PASS: one-request capture sequence, isolated modes and restoration");
}
