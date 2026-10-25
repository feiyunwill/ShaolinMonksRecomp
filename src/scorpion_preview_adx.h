#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace openshao::scorpion {
struct DecodedVoice {
    uint32_t sample_rate = 0;
    std::vector<int16_t> samples;
};

// Decode the original Xbox mono CRI ADX (type 3, 4-bit ADPCM) to signed PCM.
bool DecodeVoice(std::span<const uint8_t> adx, DecodedVoice& voice);
}
