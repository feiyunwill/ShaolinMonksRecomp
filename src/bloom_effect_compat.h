#pragma once
#include <cstdint>

namespace openshao {
// Skip only the final bloom effect composite; preserve intermediate passes and other additive draws.
struct BloomEffectDraw {
    uint32_t primitive = 0, vertices = 0, vertex_shader = 0;
    uint32_t target_width = 0, target_height = 0, source_width = 0, source_height = 0;
    uint32_t blend_source = 0, blend_destination = 0, blend_operation = 0;
    bool pixel_shader = false, depth_enabled = false, blending_enabled = false, source_is_target = false;
};

constexpr bool IsBloomEffectComposite(const BloomEffectDraw& draw) {
    return draw.primitive == 9 && draw.vertices == 4 && draw.vertex_shader == 0x104 &&
           !draw.pixel_shader && !draw.depth_enabled && draw.blending_enabled &&
           draw.blend_source == 1 && draw.blend_destination == 1 &&
           draw.blend_operation == 0x8006 && draw.source_is_target &&
           draw.target_width == 640 && draw.target_height == 480 &&
           draw.source_width == 160 && draw.source_height == 120;
}
}
