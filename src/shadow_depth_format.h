#pragma once
#include <cstdint>
#ifdef _WIN32
#include <dxgiformat.h>

namespace openshao {
struct ShadowDepthFormats {
    DXGI_FORMAT resource;
    DXGI_FORMAT view;
    DXGI_FORMAT sample;
    bool stencil;
};
bool FloatShadowDepth(bool supported, uint32_t guest_format, bool offscreen);
ShadowDepthFormats ShadowFormats(DXGI_FORMAT resource);
}
#endif
