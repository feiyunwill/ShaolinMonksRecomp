#include "scorpion_preview_mix.h"
#include <algorithm>
namespace openshao::scorpion::audio_preview {
void MixMenuVoice(float* game_stereo, const float* voice_stereo, size_t frames) {
    // Both inputs are already normalized to [-1, 1]. Attenuate the menu bed
    // while previewing so their sum never reaches digital clipping.
    constexpr float menu_gain = 0.30f;
    constexpr float preview_gain = 0.65f;
    for (size_t i = 0; i < frames * 2; ++i)
        game_stereo[i] = game_stereo[i] * menu_gain + voice_stereo[i] * preview_gain;
}
}
