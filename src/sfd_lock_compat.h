#pragma once
#include <cstdint>
#include "gpu/texture.h"

namespace openshao::sfd {

// Generic surface locks miss small flags=0 writes. Invalidate only this title's writable full-frame SFD surfaces.
inline bool NeedsWriteInvalidation(uint32_t resource, uint32_t face, uint32_t level,
                                   uint32_t rect, uint32_t flags) {
    if (!resource || face || level || rect || flags) return false;
    gpu::TextureDesc d{};
    if (!gpu::Describe(resource, d)) return false;
    return d.linear && !d.cube && d.depth == 1 && d.levels == 1 &&
           d.width == 640 && (d.height == 480 || d.height == 288) &&
           (d.format == 0x12 || d.format == 0x24);
}

}
