// Capture the pointer only for the active keyboard player; F10 and focus loss release it.
#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>

#include "controller.h"
#include "core/input.h"
#include "core/log.h"
#include "host.h"
#include "linux/linux_input.h"
#include "linux/virtual_keys.h"
#include "mouse.h"
#include "keyboard.h"
#include "input_keys_sdl.h"

namespace {
std::mutex lock;
openshao::mouse::State state;
SDL_Window* window = nullptr;
std::atomic<bool> captured{false};
bool released = false;

double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

bool Available() {
    if (openshao::options.background || !window || openshao::controller::KeyboardPort() < 0) return false;
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    return (flags & SDL_WINDOW_INPUT_FOCUS) && !(flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
}

void Release() {
    if (captured.exchange(false) && window) SDL_SetWindowRelativeMouseMode(window, false);
    std::lock_guard guard(lock);
    state.Clear();
}

bool Capture() {
    if (!Available() || !SDL_SetWindowRelativeMouseMode(window, true)) return false;
    {
        std::lock_guard guard(lock);
        state.Clear();
    }
    captured = true;
    released = false;
    return true;
}

int ButtonIndex(uint8_t button) {
    switch (button) {
    case SDL_BUTTON_LEFT: return 0;
    case SDL_BUTTON_RIGHT: return 1;
    case SDL_BUTTON_MIDDLE: return 2;
    case SDL_BUTTON_X1: return 3;
    case SDL_BUTTON_X2: return 4;
    default: return -1;
    }
}
}

namespace openshao::mouse {
bool Captured() { return captured.load(); }
void Configure(const Settings& settings) {
    auto valid = settings;
    valid.sensitivity = std::clamp(valid.sensitivity, 1, 300);
    std::lock_guard guard(lock);
    state.Configure(valid);
}
void Window(void*) {}  // linux_input::Window receives the SDL window
bool Message(void*, uint32_t, uintptr_t, intptr_t, intptr_t*) { return false; }
void Poll(input::Pad& pad) {
    if (!Captured()) return;
    std::lock_guard guard(lock);
    const auto mouse = state.Sample(Now());
    if (std::abs(int(mouse.lx)) > std::abs(int(pad.lx))) pad.lx = mouse.lx;
    if (std::abs(int(mouse.ly)) > std::abs(int(pad.ly))) pad.ly = mouse.ly;
    if (std::abs(int(mouse.rx)) > std::abs(int(pad.rx))) pad.rx = mouse.rx;
    if (std::abs(int(mouse.ry)) > std::abs(int(pad.ry))) pad.ry = mouse.ry;
    for (unsigned i = 0; i < 8; ++i) pad.analog[i] = std::max(pad.analog[i], mouse.analog[i]);
    pad.buttons |= mouse.buttons;
}
}

namespace linux_input {
using openshao::mouse::Captured;

unsigned VirtualKey(int scancode) {
    return openshao::bindings::VirtualKey(SDL_Scancode(scancode));
}

void Window(void* sdl_window) {
    Release();
    window = static_cast<SDL_Window*>(sdl_window);
    released = false;
}

void Motion(float dx, float dy) {
    if (!Captured()) return;
    std::lock_guard guard(lock);
    state.Move(int(dx), int(dy), Now());
}

void Button(uint8_t sdl_button, bool down) {
    const int index = ButtonIndex(sdl_button);
    if (index < 0) return;
    if (!Captured()) {
        if (down) Capture();  // the capturing click is not passed to the game
        return;
    }
    std::lock_guard guard(lock);
    state.Button(unsigned(index), down);
}

void Wheel(float direction) {
    if(Captured()){std::lock_guard guard(lock);state.Wheel(direction>0?1:direction<0?-1:0,Now());}
}

void ToggleCapture() {
    if (Captured()) {
        Release();
        released = true;
    } else {
        Capture();
    }
}

void KeyPressed(unsigned key) {
    if (released || Captured()) return;
    if (openshao::keyboard::Mapped(key)) Capture();
}

void FocusLost() { Release(); }

}
