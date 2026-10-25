#include <array>
#include <initializer_list>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
std::array<uint8_t, 0x800000> memory{};
unsigned calls = 0;
uint32_t submitted = 0;
bool fail = false;
void Check(bool value, const char* text) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
}
struct X86Context { uint32_t esp = 0x1000, eax = 0, ecx = 0, edi = 0; };
namespace kernel {
uint32_t R32(uint32_t at) {
    Check(at <= memory.size()-4, "valid guest read");
    uint32_t value; std::memcpy(&value, memory.data()+at, 4); return value;
}
void W32(uint32_t at, uint32_t value) {
    Check(at <= memory.size()-4, "valid guest write");
    std::memcpy(memory.data()+at, &value, 4);
}
}
#define X86_FUNC(name) void name(X86Context& ctx, uint8_t* base)
#define X86_EXTERN_FUNC(name) X86_FUNC(name)
#define LOG_FIRST(...) ((void)0)
X86_FUNC(__imp__sub_0011DC60) {
    ++calls;
    submitted = kernel::R32(ctx.esp+4);
    if (fail) throw std::runtime_error("original loader failure");
    ctx.eax = 0x81000000;
    ctx.ecx = 0x12345678;
    ctx.esp += 4;
}
X86_FUNC(__imp__sub_001B9EF0) { ctx.eax=0x3000;ctx.esp+=12; }
X86_FUNC(__imp__sub_00012F72) {
    Check(kernel::R32(0x505970)==0xA4 || kernel::R32(0x505970)==0xAC,
          "constructor observes the actual room identity");
    ctx.eax=ctx.ecx;kernel::W32(ctx.ecx,0x49E4F0);ctx.esp+=8;
}
std::array<uint32_t,4> placement{};
unsigned placements=0, target_updates=0;
namespace kernel {
uint32_t CallGuest(X86Context& ctx,uint32_t fn,std::initializer_list<uint32_t> args) {
    Check(fn==0x15DC30 && ctx.ecx==0x8000 && args.size()==1,"entry uses the native player teleport routine");
    const auto position=*args.begin();++placements;
    for(unsigned i=0;i<4;++i)placement[i]=R32(position+4*i);
    for(unsigned i=0;i<3;++i)W32(0x8080+4*i,placement[i]);
    // Deliberately clobber context to verify the enclosing hook restores it.
    ctx.eax=0xdead;ctx.ecx=0xbeef;ctx.esp=0x1234;ctx.edi=0x5678;
    return 0;
}
}
X86_FUNC(__imp__sub_001B7470) {
    ++target_updates;
    Check(ctx.ecx==0x5000 && ctx.edi==0x3456 && ctx.esp==0x1000 && ctx.eax==0x55aa,
          "entry relocation preserves context before native camera-target update");
    ctx.eax=0x1234abcd;ctx.esp+=12;
}
#include "survival5_recovery_hooks.inl"

int main() {
    kernel::W32(0x5020E8+0xA4*4, 0x4FFF20);
    kernel::W32(0x4FFF20+0x44, 0xA4);
    kernel::W32(0x4FFF20+0x48, 0x10001);
    kernel::W32(0x4FFF20+0x4C, 4213);
    kernel::W32(0x4FFF20+0x50, 4214);
    const auto invoke = [](uint32_t room, uint32_t resource) {
        kernel::W32(0x505970, room);
        kernel::W32(0x1000, 0x17AA82);
        kernel::W32(0x1004, resource);
        kernel::W32(0x1008, 0x2000);
        const auto before = calls;
        X86Context ctx;
        sub_0011DC60(ctx, nullptr);
        Check(calls == before+1, "each request invokes the original loader once");
        Check(ctx.eax == 0x81000000 && ctx.ecx == 0x12345678 && ctx.esp == 0x1004,
              "original loader results and guest ABI are preserved");
        Check(kernel::R32(0x1004) == resource && kernel::R32(0x1008) == 0x2000 &&
              kernel::R32(0x505970) == room, "caller arguments and live room identity are preserved");
        return submitted;
    };
    Check(invoke(0xA4, 4213) == 4373, "A4 recovers a original Xbox stone arena");
    Check(invoke(0xA4, 4214) == 4374, "A4 uses the matching original Xbox arena textures");
    for (uint32_t room = 0; room < 256; ++room) {
        if (room == 0xA4) continue;
        Check(invoke(room, 4213) == 4213 && invoke(room, 4214) == 4214,
              "every other room retains its resource references");
    }
    for (uint32_t resource = 3500; resource < 4400; ++resource) {
        if (resource != 4213 && resource != 4214)
            Check(invoke(0xA4, resource) == resource, "A4 gameplay, collision and other resources are unchanged");
    }
    for (const uint32_t field : {0x44u, 0x48u, 0x4Cu, 0x50u}) {
        const auto address = 0x4FFF20+field, original = kernel::R32(address);
        kernel::W32(address, original^1);
        Check(invoke(0xA4, 4213) == 4213 && invoke(0xA4, 4214) == 4214,
              "a modified room descriptor does not receive original-content recovery");
        kernel::W32(address, original);
    }
    kernel::W32(0x5020E8+0xA4*4, 0);
    Check(invoke(0xA4, 4213) == 4213, "missing or replaced room descriptors are preserved");
    kernel::W32(0x5020E8+0xA4*4, 0x4FFF20);
    kernel::W32(0x505970, 0xA4);
    kernel::W32(0x1004, 4213);
    fail = true;
    X86Context ctx;
    try { sub_0011DC60(ctx, nullptr); Check(false, "original exception propagates"); }
    catch (const std::runtime_error&) {}
    Check(kernel::R32(0x1004) == 4213 && kernel::R32(0x505970) == 0xA4,
          "caller state is restored when the original loader throws");
    fail=false;
    X86Context property;property.edi=0xA4;
    kernel::W32(0x1000,0x56BD7);kernel::W32(0x1004,0x49DED8);
    sub_001B9EF0(property,nullptr);
    Check(property.edi==0xAC && property.eax==0x3000 && property.esp==0x100c,
          "A4 initializes the native arena camera at the specific property caller");
    Check(kernel::R32(0x505970)==0xA4,"camera dispatch preserves live room ID");
    property={};property.edi=0xA4;kernel::W32(0x1000,0x56BD8);
    sub_001B9EF0(property,nullptr);Check(property.edi==0xA4,"other property callers are unchanged");
    X86Context camera;camera.ecx=0x4000;
    sub_00012F72(camera,nullptr);
    Check(camera.eax==0x4000 && camera.esp==0x1008,"original camera constructor ABI is preserved");
    const auto f=[](uint32_t a){return std::bit_cast<float>(kernel::R32(a));};
    // Independent expected eye/target offsets and combat bounds from the native Pit profile.
    Check(f(0x48B0)==999 && f(0x48B4)==-807 && f(0x48B8)==634 &&
          f(0x48C0)==115 && f(0x48C4)==-1375 && f(0x48C8)==-100,
          "camera uses native arena eye and focus offsets instead of the zero default");
    Check(f(0x48DC)==700 && f(0x48E0)==-50000 && f(0x48E4)==-6000 &&
          f(0x48EC)==11200 && f(0x48F0)==7000 && f(0x48F4)==6000 &&
          f(0x492C)==7000 && f(0x4AB0)==-150 && f(0x48AC)==69,
          "camera uses the native arena's combat bounds and projection");
    Check(kernel::R32(0x505970)==0xA4,"camera construction never changes live room identity");
    kernel::W32(0x505970,0xAC);kernel::W32(0x48B0,0x12345678);
    camera={};camera.ecx=0x4000;sub_00012F72(camera,nullptr);
    Check(kernel::R32(0x48B0)==0x12345678,"other room camera profiles are unchanged");
    const auto tick=[&](uint32_t room,uint32_t id,float y=6374.9f) {
        kernel::W32(0x505970,room);kernel::W32(0x64D230,0x8000);kernel::W32(0x8068,id);
        kernel::W32(0x8080,std::bit_cast<uint32_t>(4425.f));
        kernel::W32(0x8084,std::bit_cast<uint32_t>(y));
        kernel::W32(0x8088,std::bit_cast<uint32_t>(-1152.f));
        kernel::W32(0x1004,1);kernel::W32(0x1008,0x9000);
        X86Context frame;frame.ecx=0x5000;frame.edi=0x3456;frame.eax=0x55aa;
        const auto before=target_updates;
        sub_001B7470(frame,nullptr);
        Check(target_updates==before+1 && frame.eax==0x1234abcd && frame.esp==0x100c,
              "native target update still runs once with its original ABI");
        Check(std::bit_cast<float>(kernel::R32(0x8084))==y,"native player height is retained");
        Check(kernel::R32(0x505970)==room,"entry relocation retains the live room");
        return std::array{std::bit_cast<float>(kernel::R32(0x8080)),std::bit_cast<float>(kernel::R32(0x8088))};
    };
    Check(tick(0xA4,1)==std::array{4425.f,-1152.f},"a failed scene load does not arm entry relocation");
    invoke(0xA4,4213);
    Check(tick(0xA4,2)==std::array{4425.f,-1152.f},"another actor is not relocated");
    Check(tick(0xA3,1)==std::array{4425.f,-1152.f},"other rooms retain their native position");
    Check(tick(0xA4,1,0)==std::array{4425.f,-1152.f},"unready player placement waits for the arena");
    Check(tick(0xA4,1)==std::array{5800.f,-4600.f},"A4 starts near the owner's captured front-center point");
    Check(placement[1]==std::bit_cast<uint32_t>(6374.9f) && placement[3]==std::bit_cast<uint32_t>(1.f),
          "native teleport receives the existing height and a position vector");
    const auto once=placements;
    Check(tick(0xA4,1)==std::array{4425.f,-1152.f} && placements==once,
          "later frames leave movement and repositioning free");
    invoke(0xA4,4213);
    Check(tick(0xA4,1)==std::array{5800.f,-4600.f} && placements==once+1,
          "reentering A4 reapplies the starting point once");
    for(uint32_t room=0;room<256;++room)if(room!=0xA4) {
        invoke(room,4213);
        Check(tick(room,1)==std::array{4425.f,-1152.f},"every other room retains its original position");
    }
    std::puts("PASS: A4 scene recovery, all other rooms/resources, modified descriptors and original loader ABI");
}
