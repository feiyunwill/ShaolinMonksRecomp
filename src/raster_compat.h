#pragma once
#include <string>
namespace gpu { struct Assembled; }
namespace openshao {
bool UnitSpriteTexcoords(const gpu::Assembled& assembled);
std::string AtlasQuadHlsl(std::string source);
std::string AtlasBoundsHlsl();
}
