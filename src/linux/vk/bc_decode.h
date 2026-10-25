#pragma once
// Decode BC1/BC2/BC3 to BGRA8 when textureCompressionBC is unavailable.
// Layouts follow the Khronos Data Format Specification, S3TC compressed texture formats.
#include <cstdint>
#include <vector>

namespace vkbc {

// blocks: rows of block_pitch bytes, ceil(h / 4) rows per slice; out: BGRA rows of
// width * 4 bytes, the width and height rounded up to whole blocks
void Decode(int bc, const uint8_t* blocks, uint32_t block_pitch, uint32_t width, uint32_t height, uint32_t slices,
            std::vector<uint8_t>& out, uint32_t& out_pitch);

}
