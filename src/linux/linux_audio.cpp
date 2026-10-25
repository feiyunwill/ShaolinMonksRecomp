#include <SDL3/SDL.h>

#include <functional>
#include <vector>

#include "audio/audio.h"
#include "core/log.h"
#include "host.h"
#include "launcher.h"
#include "scorpion_preview_audio.h"

namespace audio {
namespace {
struct Output {
    std::function<void(float*, uint32_t)> render;
    std::vector<float> block = std::vector<float>(kRate / 100 * 2);
};

void SDLCALL Feed(void* data, SDL_AudioStream* stream, int additional, int) {
    auto* out = static_cast<Output*>(data);
    SDL_SetAudioStreamGain(stream, openshao::options.background ? 0.0f : openshao::launcher::AudioGain());
    constexpr uint32_t kFrames = kRate / 100;
    while (additional > 0) {
        out->render(out->block.data(), kFrames);
        openshao::scorpion::audio_preview::Mix(out->block.data(), kFrames);
        SDL_PutAudioStreamData(stream, out->block.data(), int(out->block.size() * sizeof(float)));
        additional -= int(out->block.size() * sizeof(float));
    }
}
}

void StartOutput(std::function<void(float*, uint32_t)> render) {
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        LOGE("audio: SDL audio unavailable (%s), no sound", SDL_GetError());
        return;
    }
    static Output output;  // lives for the process, as the XAudio2 thread does
    output.render = std::move(render);
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(kRate)};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, &output);
    if (!stream) {
        // Movies use the sound clock; a silent device must keep pulling audio when no device is available.
        LOGE("audio: no playback device (%s), playing silently", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO))
            stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Feed, &output);
        if (!stream) {
            LOGE("audio: no silent device either (%s)", SDL_GetError());
            return;
        }
    }
    SDL_SetAudioStreamGain(stream, openshao::options.background ? 0.0f : openshao::launcher::AudioGain());
    SDL_ResumeAudioStreamDevice(stream);
}

}
