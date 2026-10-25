#include "brightness.h"
#include "kernel/kernel.h"
#include <array>
#include <bit>
#include <iostream>

static std::array<uint8_t, 8 << 20> memory{};
static uint32_t preview;
namespace kernel { uint8_t* g_base = memory.data(); }
namespace logx { void Write(Level, const char*, ...) {} }
X86_FUNC(__imp__sub_00123A50) {
    preview = kernel::R32(ctx.esp + 4);
    kernel::Return(ctx, 123, 0);
}
X86_EXTERN_FUNC(sub_00123A50);

int main() {
    unsigned failures = 0;
    const auto check = [&](bool good, const char* message) {
        if (!good) { ++failures; std::cerr << message << '\n'; }
    };
    const auto half = std::bit_cast<uint32_t>(.5f);
    openshao::brightness::Initialize();
    check(kernel::R32(0x4DF5C0) == half, "A clean mapped image must start at 50 percent brightness");
    const auto saved = std::bit_cast<uint32_t>(.23f);
    kernel::W32(0x4DF5C0, saved);
    X86Context ctx{};
    const auto apply = [&](uint32_t caller, uint32_t value) {
        ctx.esp = 0x1000;
        kernel::W32(ctx.esp, caller);
        kernel::W32(ctx.esp + 4, value);
        sub_00123A50(ctx, memory.data());
        check(ctx.esp == 0x1004 && ctx.eax == 123, "Brightness overrides must preserve native return and cdecl cleanup");
    };
    for (const auto value : {0.f, .23f, .5f, 1.f}) {
        const auto bits = std::bit_cast<uint32_t>(value);
        kernel::W32(0x598308, bits);
        apply(0x4C17F, bits);
        check(preview == bits && kernel::R32(0x598308) == bits,
              "Ordinary brightness previews must retain the player's complete adjustment range, including zero");
    }
    kernel::W32(0x598308, 0);
    apply(0x4BDD3, 0);
    check(preview == half && kernel::R32(0x598308) == half,
          "The original Default action must set both the brightness draft and rendered preview to 50 percent");
    check(kernel::R32(0x4DF5C0) == saved, "Restoring a preview must preserve saved brightness until native confirmation");
    apply(0x4C0E4, saved);
    check(preview == saved, "Native cancellation must restore the existing saved brightness");
    return failures ? 1 : 0;
}
