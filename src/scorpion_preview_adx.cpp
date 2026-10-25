#include "scorpion_preview_adx.h"
#include <algorithm>
#include <cmath>

namespace openshao::scorpion {
namespace {
uint32_t Big32(std::span<const uint8_t> data, size_t at) {
    return uint32_t(data[at]) << 24 | uint32_t(data[at + 1]) << 16 | uint32_t(data[at + 2]) << 8 | data[at + 3];
}
uint16_t Big16(std::span<const uint8_t> data, size_t at) {
    return uint16_t(data[at] << 8 | data[at + 1]);
}
}

bool DecodeVoice(std::span<const uint8_t> adx, DecodedVoice& voice) {
    voice = {};
    if (adx.size() < 40 || Big16(adx, 0) != 0x8000 || adx[4] != 3 ||
        adx[5] != 18 || adx[6] != 4 || adx[7] != 1) return false;
    const uint32_t rate = Big32(adx, 8), count = Big32(adx, 12);
    const uint32_t cutoff = Big16(adx, 16), start = Big16(adx, 2) + 4;
    if (rate < 8000 || rate > 96000 || count == 0 || count > 96000 * 60 ||
        cutoff == 0 || cutoff >= rate / 2 || start < 36 || start > adx.size()) return false;
    const uint64_t blocks = (uint64_t(count) + 31) / 32;
    if (blocks > (adx.size() - start) / 18) return false;

    constexpr double pi = 3.14159265358979323846;
    const double a = std::sqrt(2.) - std::cos(2. * pi * cutoff / rate);
    const double b = std::sqrt(2.) - 1.;
    const double c = (a - std::sqrt((a + b) * (a - b))) / b;
    const int32_t coefficient1 = int32_t(c * 2. * 4096.);
    const int32_t coefficient2 = int32_t(-c * c * 4096.);
    std::vector<int16_t> samples(count);
    int32_t previous1 = 0, previous2 = 0;
    for (uint64_t block = 0; block < blocks; ++block) {
        const size_t at = start + size_t(block * 18);
        const int32_t scale = Big16(adx, at);
        for (unsigned i = 0; i != 32 && block * 32 + i < count; ++i) {
            const uint8_t packed = adx[at + 2 + i / 2];
            const unsigned nibble = (i & 1) ? packed & 15 : packed >> 4;
            const int32_t delta = int32_t(nibble) - (nibble < 8 ? 0 : 16);
            const int64_t predictor = int64_t(coefficient1) * previous1 + int64_t(coefficient2) * previous2;
            const int32_t sample = std::clamp(int32_t(delta * scale + (predictor >> 12)), -32768, 32767);
            previous2 = previous1;
            previous1 = sample;
            samples[size_t(block * 32 + i)] = int16_t(sample);
        }
    }
    voice.sample_rate = rate;
    voice.samples = std::move(samples);
    return true;
}
}
