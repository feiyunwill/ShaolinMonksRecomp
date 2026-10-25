#include "scorpion_preview_adx.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using openshao::scorpion::DecodedVoice;
    using openshao::scorpion::DecodeVoice;
    int failures = 0;
    const auto check = [&](bool valid, const char* message) {
        if (!valid) { ++failures; std::cerr << message << '\n'; }
    };
    std::vector<uint8_t> adx(40 + 2 * 18 + 18);
    adx[0] = 0x80;
    adx[3] = 36; // ADX payload starts at 0x28
    adx[4] = 3;
    adx[5] = 18;
    adx[6] = 4;
    adx[7] = 1;
    adx[8] = 0x00; adx[9] = 0x00; adx[10] = 0x5D; adx[11] = 0xC0; // 24kHz
    adx[15] = 33; // two blocks, one sample in the last block
    adx[16] = 1; adx[17] = 0xF4; // lowpass cutoff 500Hz
    adx[41] = 1;
    adx[42] = 0x10;
    adx[59] = 1;
    adx[60] = 0x20;
    adx[76] = 0x80; adx[77] = 1;
    DecodedVoice voice;
    check(DecodeVoice(adx, voice) && voice.sample_rate == 24000 && voice.samples.size() == 33,
          "Two ADX blocks must decode exactly the advertised sample count");
    check(voice.samples.size() >= 2 && voice.samples[0] == 1 && voice.samples[1] >= 0,
          "ADX nibble order and predictor must decode a known frame");
    auto invalid = adx;
    invalid[7] = 2;
    check(!DecodeVoice(invalid, voice) && voice.samples.empty(),
          "Stereo ADX must be rejected instead of being decoded as mono");
    invalid = adx;
    invalid.resize(40 + 18);
    check(!DecodeVoice(invalid, voice), "Truncated ADX data must be rejected");
    invalid = adx;
    invalid[0] = 0;
    check(!DecodeVoice(invalid, voice), "An invalid ADX signature must be rejected");
    invalid = adx;
    invalid[15] = 0;
    check(!DecodeVoice(invalid, voice), "Zero length ADX must be rejected");
    std::cout << (failures ? "FAIL" : "PASS") << ": CRI ADX validation, duration and decoding\n";
    return failures ? 1 : 0;
}
