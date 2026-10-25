#pragma once
#include <cstdint>
#include <string>
#ifdef _WIN32
struct D3D12_SAMPLER_DESC;
#endif

namespace openshao {
constexpr uint8_t kShadowManual = 0x80;
constexpr uint8_t kShadowLinear = 0x40;

constexpr uint8_t ShadowSamplingMode(uint8_t kind, bool linear, bool shader_compare) {
    return kind && shader_compare ? uint8_t(kind | kShadowManual | (linear ? kShadowLinear : 0)) : kind;
}

bool ShaderShadowCompareEnabled();
std::string ShadowComparisonHlsl();
bool RasterDepthClearEnabled();
bool UseRasterDepthClear(bool enabled, bool offscreen, bool depth, bool stencil);
std::string RasterDepthClearHlsl();
#ifdef _WIN32
void ConfigureShadowSampler(D3D12_SAMPLER_DESC& sampler, uint8_t mode);
#endif
}
