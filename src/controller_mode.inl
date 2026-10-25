#include "game_preferences.h"
#include "recomp_settings.h"
#include "ed_nexus.h"
// The native setter uses 0=solo, 1=Ko-op, 2=versus.
// Character selection precedes it; inspect the guest stack from XInputGetState before Poll.
void SyncRetailFrontendMode() {
    // The original effect dispatcher separately gates each player at this mask.
    // Keep hardware capabilities stable and control effects with the saved host preference.
    const auto flags=kernel::R32(0x639674);
    const auto desired=(flags&~0xFu)|(openshao::controller::RumbleEnabled()?0xFu:0u);
    if(flags!=desired)kernel::W32(0x639674,desired);
    const uint32_t depth=kernel::R32(0x6C9308);
    if (!depth || depth>16) return;
    const uint32_t top=kernel::R32(0x6C9308+depth*4);
    if (top==0x598430) openshao::controller::SetMultiplayer(false);
    else if (top==0x598DF0) {
        const uint32_t selected=kernel::R32(0x598430+0x38);
        openshao::controller::SetMultiplayer(selected==1 || selected==2);
    }
}
X86_EXTERN_FUNC(__imp__sub_00015C86);
X86_FUNC(sub_00015C86) {
    const uint32_t menu=ctx.ecx;
    openshao::ed_nexus::FrontendInput(ctx,base);
    if(openshao::recomp::MainInput(ctx,base))return;
    __imp__sub_00015C86(ctx,base);
    LOG_FIRST(4,"input: retail menu callback this=%08X",menu);
    if (menu!=0x598430) return;
    openshao::preferences::Apply(ctx, base);
    const uint32_t depth=kernel::R32(0x6C9308);
    if (!depth || depth>16) return;
    const uint32_t top=kernel::R32(0x6C9308+depth*4);
    static uint64_t last_menu=~uint64_t{};
    const uint64_t state=(uint64_t(top)<<32)|kernel::R32(menu+0x38);
    if (state!=last_menu) {LOGI("input: retail menu top=%08X selected=%u depth=%u",top,uint32_t(state),depth);last_menu=state;}
    SyncRetailFrontendMode();
}
X86_EXTERN_FUNC(__imp__sub_0001670D);
X86_FUNC(sub_0001670D) {
    const uint32_t mode=kernel::R32(ctx.esp+4);
    __imp__sub_0001670D(ctx,base);
    if (mode<=2) openshao::controller::SetMultiplayer(mode!=0);
}
