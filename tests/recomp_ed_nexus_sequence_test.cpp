#include "ed_nexus_sequence.h"
#include <array>
#include <cstdio>
#include <cstdlib>

using openshao::ed_nexus::Sequence;
constexpr std::array<uint16_t,10> code = {0x1000,0x1000,0x1000,0x1000,0x4000,0x4000,0x4000,0x4000,0x80,0x20};
void Check(bool value, const char* text) {
    if (!value) { std::fprintf(stderr,"FAIL: %s\n",text); std::exit(1); }
}
bool Enter(Sequence& state, bool main = true, bool modifier = true) {
    bool fired = false;
    for (auto button : code) {
        fired |= state.Feed(button,main,modifier);
        state.Feed(0,main,modifier);
    }
    return fired;
}
int main() {
    Sequence state;
    Check(!state.Unlocked(),"Nexus starts locked");
    Check(Enter(state) && state.Unlocked(),"four up, four down, X then B unlocks Nexus");
    Check(!Enter(state),"unlock notification fires once per session");
    state.Feed(0,false,false);
    Check(state.Unlocked(),"leaving the frontend preserves the session unlock");

    Sequence elsewhere;
    Check(!Enter(elsewhere,false) && !elsewhere.Unlocked(),"the sequence cannot unlock in gameplay or another menu");
    Sequence missing;
    Check(!Enter(missing,true,false) && !missing.Unlocked(),"the complete code is rejected without the held modifier");
    Sequence released;
    for (unsigned i=0;i<8;++i) released.Feed(code[i],true,true);
    released.Feed(0,true,false);
    released.Feed(0x80,true,true); released.Feed(0x20,true,true);
    Check(!released.Unlocked(),"releasing the modifier discards the partial sequence");
    Check(Enter(released),"the complete code works after holding the modifier again");

    Sequence chord;
    for (unsigned i=0;i<8;++i) chord.Feed(code[i],true,true);
    Check(!chord.Feed(0xa0,true,true) && !chord.Unlocked(),"X and B together do not match separate presses");
    Check(!chord.Feed(0x20,true,true),"a rejected chord clears the prefix");
    Sequence wrong;
    for (unsigned i=0;i<4;++i) wrong.Feed(code[i],true,true);
    wrong.Feed(0x40,true,true);
    for (unsigned i=4;i<10;++i) wrong.Feed(code[i],true,true);
    Check(!wrong.Unlocked(),"an unrelated button breaks the sequence");
    Check(Enter(wrong),"a valid sequence still works after an error");

    Sequence held;
    held.Feed(code[0],true,true);
    for (unsigned i=0;i<20;++i) held.Feed(0,true,true);
    for (unsigned i=4;i<10;++i) held.Feed(code[i],true,true);
    Check(!held.Unlocked(),"holding a direction supplies only its first pressed edge");

    Sequence transition;
    for (unsigned i=0;i<8;++i) transition.Feed(code[i],true,true);
    transition.Feed(0,false,false);
    transition.Feed(0x80,true,true); transition.Feed(0x20,true,true);
    Check(!transition.Unlocked(),"menu transitions discard a partial sequence");

    Sequence expired;
    for (unsigned i=0;i<8;++i) expired.Feed(code[i],true,true);
    for (unsigned i=0;i<Sequence::kIdleLimit+1;++i) expired.Feed(0,true,true);
    expired.Feed(0x80,true,true); expired.Feed(0x20,true,true);
    Check(!expired.Unlocked(),"an expired partial sequence cannot be completed");

    Sequence suffix;
    suffix.Feed(0x1000,true,true);
    Check(Enter(suffix),"overlapping direction prefixes remain usable");
    Check(!openshao::ed_nexus::OpenShortcut(false,true,false,0x80),"pause X is inactive while locked");
    Check(!openshao::ed_nexus::OpenShortcut(true,false,false,0x80),"X outside the native pause does not open Nexus");
    Check(!openshao::ed_nexus::OpenShortcut(true,true,true,0x80),"X does not reopen an already active Nexus");
    Check(!openshao::ed_nexus::OpenShortcut(true,true,false,0xa0),"pause chords preserve their original handling");
    Check(openshao::ed_nexus::OpenShortcut(true,true,false,0x80),"unlocked pause X opens the Nexus");
    std::puts("PASS: secret sequence, session lifetime, input isolation and pause access");
}
