#include "kernel/kernel.h"
#include "launcher.h"
#include "scorpion_voice.h"
#include "scorpion_preview_audio.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

static std::array<uint8_t, 1 << 20> memory{};
static openshao::launcher::Settings settings;
static uint32_t emitted, calls, checked_pages;
static bool actor_committed = true;
static std::vector<unsigned> preview_events;
static unsigned preview_stop_count = 0;
static bool preview_asset_present = true;
namespace kernel { uint8_t* g_base = memory.data(); }
namespace openshao::scorpion::audio_preview {
bool Play(unsigned index) { preview_events.push_back(index); return preview_asset_present; }
void Stop() { ++preview_stop_count; }
}
namespace logx { void Write(Level, const char*, ...) {} }
namespace mem {
bool IsCommitted(uint32_t address) {
    ++checked_pages;
    return actor_committed && address >= 0x10000 && address < memory.size();
}
}
namespace openshao::launcher { const Settings& Current() { return settings; } }
X86_FUNC(__imp__sub_00151B90) {
    ++calls;
    emitted = kernel::R32(ctx.esp + 8);
    kernel::Return(ctx, 0xCAFE, 7);
    ctx.ecx = 0xFEED;
}
X86_EXTERN_FUNC(sub_00151B90);

int main() {
    unsigned failures = 0;
    auto check = [&](bool good, const char* message) {
        if (!good) { ++failures; std::cerr << message << '\n'; }
    };
    constexpr uint32_t actor = 0x20000;
    constexpr std::array<uint32_t, 4> lines{0x12F19BF6, 0x24E7FCEE, 0xB9A3E56F, 0xFE039FBF};
    auto populate = [&] {
        memory[actor + 0x234F] = 4;
        for (unsigned i = 0; i < 4; ++i) kernel::W32(actor + 0x23E8 + i * 4, lines[i]);
    };
    auto emit = [&](uint32_t caller, uint32_t line, uint32_t instance = actor) {
        X86Context ctx{};
        ctx.esp = 0x1000; ctx.esi = instance; ctx.ecx = 0x653BC0;
        kernel::W32(ctx.esp, caller); kernel::W32(ctx.esp + 4, 0xFFFFFFFF);
        kernel::W32(ctx.esp + 8, line); kernel::W32(ctx.esp + 12, 0x3F800000);
        const auto before = calls;
        sub_00151B90(ctx, memory.data());
        check(calls == before + 1, "The enhancement must emit exactly one native voice request");
        check(ctx.esp == 0x1020 && ctx.eax == 0xCAFE && ctx.ecx == 0xFEED && ctx.esi == instance,
              "Voice selection must preserve the native result, register changes, and RET28 ABI");
        check(kernel::R32(0x1008) == line && kernel::R32(0x1004) == 0xFFFFFFFF && kernel::R32(0x100C) == 0x3F800000,
              "A substituted voice must restore its argument without changing gain or handle");
        return emitted;
    };
    populate();
    for (auto line : lines) {
        check(emit(0x17E956, line) == line, "Disabled Scorpion enhancement must preserve every native selection");
    }
    check(checked_pages == 0, "Disabled selection must not inspect or modify the actor");
    settings.scorpion_unfiltered = true;
    for (auto line : lines) {
        check(emit(0x123456, line) == line, "Other sound callers must keep their original lines");
    }
    check(checked_pages == 0, "Other sound callers must not inspect the actor");
    check(emit(0x17E956, 0x1234) == 0x1234, "Other queued sounds must retain their native selection");
    std::array<unsigned, 4> counts{};
    for (unsigned i = 0; i < 10000; ++i) {
        const auto line = emit(0x17E956, lines[0]);
        bool known = false;
        for (unsigned j = 0; j < 4; ++j) if (line == lines[j]) { ++counts[j]; known = true; }
        check(known, "Enhanced spear dialogue must use only the four existing Scorpion recordings");
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (i < 2) check(counts[i] >= 350 && counts[i] <= 650,
            "Each common line should appear about 5 percent of the time");
        else if (i == 2) check(counts[i] >= 4650 && counts[i] <= 5350,
            "Get Over Here 1 should appear about 50 percent of the time");
        else check(counts[i] >= 3650 && counts[i] <= 4350,
            "Get Over Here 2 should appear about 40 percent of the time");
    }
    for (const auto count : {0, 255}) {
        memory[actor + 0x234F] = uint8_t(count);
        check(emit(0x17E956, lines[0]) == lines[0], "Empty or implausibly long voice queues must remain unchanged");
    }
    for (const auto count : {1, 3, 5}) {
        populate();
        memory[actor + 0x234F] = uint8_t(count);
        unsigned rare = 0;
        for (unsigned i = 0; i < 256; ++i) {
            const auto chosen = emit(0x17E956, lines[0]);
            check(std::find(lines.begin(), lines.end(), chosen) != lines.end(),
                  "Partially built Scorpion queues must still select only original lines");
            rare += chosen == lines[2] || chosen == lines[3];
        }
        check(rare > 200 && rare < 253, "A partial Scorpion queue must retain the 90 percent combined rare speech weighting");
    }
    populate();
    for (const auto caller : {0x17E3BD, 0x17E438, 0x17E681, 0x17E71A, 0x17E797}) {
        unsigned rare = 0;
        for (unsigned i = 0; i < 256; ++i) {
            const auto chosen = emit(caller, lines[0], 0);
            check(std::find(lines.begin(), lines.end(), chosen) != lines.end(),
                  "Other combat audio paths must retain only the four original Scorpion lines");
            rare += chosen == lines[2] || chosen == lines[3];
        }
        check(rare > 200 && rare < 253, "Every native combat sound path must apply the same 90 percent rare weighting");
    }
    check(emit(0x173836, lines[0]) == lines[0], "Scripted and unrelated sound callers must remain unchanged");
    populate();
    kernel::W32(actor + 0x23E8, lines[1]); kernel::W32(actor + 0x23EC, lines[0]);
    check(emit(0x17E956, lines[0]) == lines[0], "The enhancement must require the native Scorpion default sound");
    populate();
    actor_committed = false;
    check(emit(0x17E956, lines[0]) == lines[0], "Uncommitted actor memory must safely preserve native playback");
    actor_committed = true;
    check(emit(0x17E956, lines[0], 0) == lines[0], "A null actor must never be dereferenced");
    check(emit(0x17E956, lines[0], 0xFFFFF000) == lines[0], "Actor pointer arithmetic must not wrap");
    X86Context preview_ctx{};preview_ctx.ecx=0x1234;preview_ctx.esp=0x9000;
    using openshao::scorpion::Preview;
    using openshao::scorpion::StopPreview;
    check(Preview(preview_ctx,false) && Preview(preview_ctx,false) && Preview(preview_ctx,true) && Preview(preview_ctx,true),
          "Selected recordings must support both normal and rare preview pairs");
    check(preview_events==std::vector<unsigned>{0,1,2,3},
          "Normal previews start with Come Here; enhanced previews start with the first rare variant");
    check(preview_stop_count==3,
          "Each successive preview must stop exactly the previously requested sample");
    check(preview_ctx.ecx==0x1234 && preview_ctx.esp==0x9000,
          "Host preview playback must not mutate the guest CPU registers");
    StopPreview(preview_ctx);
    check(preview_stop_count==4,"Leaving enhancements must stop the playing preview");
    preview_asset_present=false;
    check(!Preview(preview_ctx,false),"An unreadable ADX must report a preview failure");
    StopPreview(preview_ctx);
    check(preview_stop_count==4,"A failed preview must not stop an invalid sound instance");
    preview_asset_present=true;
    check(Preview(preview_ctx,false) && preview_events.back()==0,
          "A failed preview must not skip the corresponding voice variation");
    StopPreview(preview_ctx);
    check(preview_stop_count==5,"Final successful recording must stop on menu close");
    std::cout << (failures ? "FAIL" : "PASS") << ": isolated Scorpion selection, four recordings, and host preview\n";
    return failures ? 1 : 0;
}
