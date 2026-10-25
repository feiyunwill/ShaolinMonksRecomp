#include <cstdio>
#include <initializer_list>
#include "shadow_depth_format.h"

int main() {
    for (uint32_t format : {0x2Cu, 0x2Du, 0x30u, 0x31u}) {
        if (!openshao::FloatShadowDepth(true, format, true)) {
            std::puts("FAIL: offscreen depth-only maps must use D32 on every capable adapter"); return 1;
        }
        if (openshao::FloatShadowDepth(true, format, false) || openshao::FloatShadowDepth(false, format, true)) {
            std::puts("FAIL: screen-sized buffers and adapters without D32 sampling must keep D24S8"); return 1;
        }
    }
    for (uint32_t format : {0x2Au, 0x2Bu, 0x2Eu, 0x2Fu, 0x06u})
        if (openshao::FloatShadowDepth(true, format, true)) {
            std::puts("FAIL: preserve stencil formats and color targets"); return 1;
        }
    const auto d24 = openshao::ShadowFormats(DXGI_FORMAT_R24G8_TYPELESS);
    const auto d32 = openshao::ShadowFormats(DXGI_FORMAT_R32_TYPELESS);
    if (d24.resource != DXGI_FORMAT_R24G8_TYPELESS || d24.view != DXGI_FORMAT_D24_UNORM_S8_UINT ||
        d24.sample != DXGI_FORMAT_R24_UNORM_X8_TYPELESS || !d24.stencil ||
        d32.resource != DXGI_FORMAT_R32_TYPELESS || d32.view != DXGI_FORMAT_D32_FLOAT ||
        d32.sample != DXGI_FORMAT_R32_FLOAT || d32.stencil) {
        std::puts("FAIL: resources, depth views, pipeline formats and sampled views must agree"); return 1;
    }
    std::puts("PASS: D32 offscreen depth-only maps, capability guard, stencil preservation and matching D24/D32 views");
}
