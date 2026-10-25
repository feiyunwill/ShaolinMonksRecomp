#pragma once
#include "bloom_effect_compat.h"
#include "gpu/state.h"
#include "gpu/texture.h"

namespace openshao {
inline bool IsOriginalBloomEffectComposite(const gpu::State& state, uint32_t primitive,
                                     uint32_t vertices, bool source_is_target) {
    if (!source_is_target || !state.rt.data || !state.textures[0] ||
        primitive != 9 || vertices != 4 || state.vs_handle != 0x104 ||
        state.ps && state.ps->handle != 0 || state.rs[gpu::kRsZEnable] != 0)
        return false;
    const auto simple = [&](uint32_t method) { return state.simple[method >> 2]; };
    if (!simple(gpu::kMBlendEnable) || simple(gpu::kMSrcBlend) != 1 ||
        simple(gpu::kMDestBlend) != 1 || simple(gpu::kMBlendOp) != 0x8006)
        return false;
    gpu::TextureDesc source;
    if (!gpu::Describe(state.textures[0], source)) return false;
    uint32_t target_width = 0, target_height = 0;
    gpu::SurfaceSize(state.rt, target_width, target_height);
    return IsBloomEffectComposite({
        primitive, vertices, state.vs_handle,
        target_width, target_height, source.width, source.height,
        simple(gpu::kMSrcBlend), simple(gpu::kMDestBlend), simple(gpu::kMBlendOp),
        state.ps && state.ps->handle != 0, state.rs[gpu::kRsZEnable] != 0,
        simple(gpu::kMBlendEnable) != 0, source_is_target,
    });
}
}
