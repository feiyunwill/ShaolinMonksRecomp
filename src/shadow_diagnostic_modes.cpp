#include "shadow_diagnostic_modes.h"
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <d3d12.h>
#endif

namespace openshao {
bool RasterDepthClearEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("OPENSHAO_RASTER_DEPTH_CLEAR");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

bool UseRasterDepthClear(bool enabled, bool offscreen, bool depth, bool stencil) {
    // Screen buffers and clears that include stencil retain the native path.
    return enabled && offscreen && depth && !stencil;
}

std::string RasterDepthClearHlsl() {
    return R"(
cbuffer ClearValue : register(b0) { float4 clear_value; };
float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 p = id == 0 ? float2(-1, -1) : id == 1 ? float2(-1, 3) : float2(3, -1);
    return float4(p, 0, 1);
}
float psmain() : SV_Depth { return saturate(clear_value.x); }
)";
}

bool ShaderShadowCompareEnabled() {
    // Keep the process default stable; capture variants use separate shader and sampler keys.
    static const bool enabled = [] {
        const char* value = std::getenv("OPENSHAO_SHADOW_SHADER_COMPARE");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

#ifdef _WIN32
void ConfigureShadowSampler(D3D12_SAMPLER_DESC& sampler, uint8_t mode) {
    if (!(mode & kShadowManual)) return;
    // Each tap must return an unfiltered depth, not a filtered comparison result.
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxAnisotropy = 1;
}
#endif

std::string ShadowComparisonHlsl() {
    return R"(
float shadow_compare(float depth, float reference) {
    int f = (int)misc.z;
    // The Xbox compares texel OP reference; native comparison samplers reverse this order.
    return f == 0 ? 0 : f == 1 ? depth < reference : f == 2 ? depth == reference :
           f == 3 ? depth <= reference : f == 4 ? depth > reference :
           f == 5 ? depth != reference : f == 6 ? depth >= reference : 1;
}
float4 shadow(Texture2D<float> t, SamplerState s, float2 uv, float z, int fmt, float2 px, float2 size) {
    float maximum = (fmt & 3) == 2 ? 65535.0 : 16777215.0;
    float reference = clamp(z, 0, maximum) / maximum;
    if ((fmt & 64) == 0)
        return shadow_compare(t.SampleLevel(s, uv, 0), reference).xxxx;
    uint width, height;
    t.GetDimensions(width, height);
    float2 extent = float2(width, height);
    float2 position = uv * extent - 0.5;
    float2 first = (floor(position) + 0.5) / extent;
    float2 step = 1 / extent;
    float2 weight = frac(position);
    // Filter the comparison results. Filtering depth before comparison loses the silhouette.
    float a = shadow_compare(t.SampleLevel(s, first, 0), reference);
    float b = shadow_compare(t.SampleLevel(s, first + float2(step.x, 0), 0), reference);
    float c = shadow_compare(t.SampleLevel(s, first + float2(0, step.y), 0), reference);
    float d = shadow_compare(t.SampleLevel(s, first + step, 0), reference);
    return lerp(lerp(a, b, weight.x), lerp(c, d, weight.x), weight.y).xxxx;
}
)";
}
}
