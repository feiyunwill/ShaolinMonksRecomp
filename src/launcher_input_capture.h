#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "input_bindings.h"

namespace openshao::launcher {
// Open controllers only after an interactive request to capture a binding.
class ControllerCapture {
    bool active=false, neutral=false;
    std::vector<SDL_Gamepad*> pads;
    uint64_t started=0;
public:
    std::string status;
    ControllerCapture()=default;
    ControllerCapture(const ControllerCapture&)=delete;
    ControllerCapture& operator=(const ControllerCapture&)=delete;
    ~ControllerCapture(){Stop();}
    bool Start(bool allowed) {
        Stop();
        if(!allowed){status="Controller capture is unavailable in background mode.";return false;}
        if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD)){status=SDL_GetError();return false;}
        active=true;started=SDL_GetTicks();neutral=false;status="Release the controls, then press a button or move an axis.";return true;
    }
    void Stop(){for(auto* pad:pads)SDL_CloseGamepad(pad);pads.clear();if(active){SDL_QuitSubSystem(SDL_INIT_GAMEPAD);active=false;}}
    bool Active()const{return active;}
    std::string Poll() {
        if(!active)return {};
        if(SDL_GetTicks()-started>15000){Stop();status="Capture timed out. Try again when the controller is connected.";return {};}
        SDL_PumpEvents();
        int count=0;auto* ids=SDL_GetGamepads(&count);std::string result;
        bool any=false;
        for(auto it=pads.begin();it!=pads.end();) {
            if(!SDL_GamepadConnected(*it)){SDL_CloseGamepad(*it);it=pads.erase(it);}else ++it;
        }
        for(int i=0;i<count;++i)if(std::none_of(pads.begin(),pads.end(),[&](auto* pad){return SDL_GetGamepadID(pad)==ids[i];}))
            if(auto* pad=SDL_OpenGamepad(ids[i]))pads.push_back(pad);
        for(auto* pad:pads) {
            for(int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;++b)if(SDL_GetGamepadButton(pad,SDL_GamepadButton(b))) {
                any=true;if(result.empty())if(const char* name=SDL_GetGamepadStringForButton(SDL_GamepadButton(b)))result=name;
            }
            for(int a=0;a<SDL_GAMEPAD_AXIS_COUNT;++a) {
                const int value=SDL_GetGamepadAxis(pad,SDL_GamepadAxis(a));
                if(a>=4?value>16000:std::abs(value)>20000) {
                    any=true;
                    if(result.empty()) {
                        const char* names[]={"leftx","lefty","rightx","righty","lefttrigger","righttrigger"};
                        result=names[a];if(a<4)result+=value<0?'-':'+';
                    }
                }
            }
        }
        SDL_free(ids);
        if(!any)neutral=true;
        if(!neutral || !bindings::ValidSource(result))return {};
        Stop();status.clear();return result;
    }
};
}
