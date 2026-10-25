#include "recomp_input.h"
#include <iostream>
int main() {
    using namespace openshao::recomp::raw;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}};
    Active(true);Key(13,true);Begin(Device::Keyboard);Key('Z',true);
    check(Take().device==Device::None,"The opening key must not leak into capture");
    Key('Z',false);Key(13,false);Key('Z',true);
    auto captured=Take();check(captured.device==Device::Keyboard && captured.key=='Z',"Capture must accept a previously unmapped key after release");
    check(Navigation()==0,"Captured input must not select another menu action");
    Begin(Device::Controller);Controller("a",0x40);
    check(Take().device==Device::None,"Controller capture must wait for neutral");
    Controller("",0);Controller("righty-",0);
    captured=Take();check(captured.source=="righty-","Raw capture must retain analog direction");
    Begin(Device::Keyboard);FocusLost();
    check(Capturing()==Device::None && Navigation()==0,"Focus loss must cancel capture and clear pending navigation");
    Active(true);Key(38,true);Key(13,true);
    check(Navigation()==0x1040 && Navigation()==0,"Fixed recovery navigation must be consumed exactly once");
    Begin(Device::Keyboard);Key(38,false);Key(13,false);Key(0x78,true);
    check(Take().device==Device::None,"F9 must remain reserved for the performance overlay");
    Cancel();Active(false);check(!Key('Z',true),"Closed menus must not consume gameplay keys");
    return failures?1:0;
}
