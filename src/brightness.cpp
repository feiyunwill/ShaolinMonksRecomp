#include "brightness.h"
#include "kernel/kernel.h"
#include <bit>

namespace openshao::brightness {
void Initialize() {
    // Seed the mapped image before guest startup. Native profile loading can replace this value.
    kernel::W32(0x4DF5C0, std::bit_cast<uint32_t>(DefaultPercent / 100.f));
    LOGR("Brightness: initial default=%d percent", DefaultPercent);
}
}

X86_EXTERN_FUNC(__imp__sub_00123A50);
X86_FUNC(sub_00123A50) {
    if (kernel::R32(ctx.esp) == 0x4BDD3) {
        // The native brightness screen's Default action resets its draft before applying the preview.
        // Keep confirmation and cancellation in the retail handler; do not change the saved value here.
        const auto value = std::bit_cast<uint32_t>(openshao::brightness::DefaultPercent / 100.f);
        kernel::W32(0x598308, value);
        kernel::W32(ctx.esp + 4, value);
        LOGR("Brightness: native Default preview=%d percent", openshao::brightness::DefaultPercent);
    }
    __imp__sub_00123A50(ctx, base);
}
