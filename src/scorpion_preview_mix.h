#pragma once
#include <cstddef>
namespace openshao::scorpion::audio_preview {
void MixMenuVoice(float* game_stereo, const float* voice_stereo, size_t frames);
}
