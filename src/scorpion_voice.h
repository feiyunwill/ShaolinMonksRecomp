#pragma once
#include <array>
#include <cstdint>
struct X86Context;

namespace openshao::scorpion {
// Event IDs resolve through the selected native voice bank, including installed voice packs.
inline constexpr std::array<uint32_t, 4> Lines{0x12F19BF6, 0x24E7FCEE, 0xB9A3E56F, 0xFE039FBF};
bool Preview(X86Context& ctx, bool enabled);
void StopPreview(X86Context& ctx);
}
