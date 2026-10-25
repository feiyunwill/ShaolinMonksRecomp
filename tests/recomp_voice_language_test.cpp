#include "kernel/kernel.h"
#include <array>
#include <iostream>
static std::array<uint8_t,8<<20> memory{};
static unsigned voice=0;
namespace logx {void Write(Level,const char*,...) {}}
namespace kernel {uint8_t* g_base=memory.data();}
namespace openshao::launcher {unsigned Language(){return 4;}unsigned VoiceLanguageIndex(){return voice;}}
X86_EXTERN_FUNC(sub_0010F0E0);
X86_EXTERN_FUNC(sub_00014A75);
int main() {
    unsigned failed=0;auto check=[&](bool good,const char* message){if(!good){++failed;std::cerr<<message<<'\n';}};
    kernel::W32(0x5B04AC,4);
    for(voice=0;voice<5;++voice) {
        X86Context ctx{};ctx.esp=0x1000;kernel::W32(ctx.esp,0xCF575);sub_0010F0E0(ctx,memory.data());
        check(ctx.eax==voice && ctx.esp==0x1004,"The shared voice bank must use the selected voice language and retain RET 0");
        for(auto address:{0xCD707u,0x1B4CF8u}) {
            ctx.esp=0x1000;kernel::W32(ctx.esp,address);sub_00014A75(ctx,memory.data());
            check(ctx.eax==voice,"Level voice banks and movie audio must follow the voice language");
        }
        for(auto address:{0xC3D35u,0x76BE6u,0x114F89u,0x123456u}) {
            ctx.esp=0x1000;kernel::W32(ctx.esp,address);sub_00014A75(ctx,memory.data());
            check(ctx.eax==4,"Text tables, fonts and other language queries must retain Spanish regardless of voice language");
        }
        ctx.esp=0x1000;kernel::W32(ctx.esp,0x114D91);sub_0010F0E0(ctx,memory.data());
        check(ctx.eax==4,"Initial console language must still initialize Spanish text");
        check(kernel::R32(0x5B04AC)==4,"Voice selection must never change the native text-language global");
    }
    return failed?1:0;
}
