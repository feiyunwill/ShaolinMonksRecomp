#pragma once
#include <cstdint>

namespace linux_input {

// Windows virtual-key code for an SDL scancode, 0 when the game does not map it.
unsigned VirtualKey(int scancode);

// main thread, from the SDL event loop
void Window(void* sdl_window);
void Motion(float dx, float dy);
void Button(uint8_t sdl_button, bool down);
void Wheel(float direction);
void ToggleCapture();   // F10
void KeyPressed(unsigned virtual_key);
void FocusLost();

}
