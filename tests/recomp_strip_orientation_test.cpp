#include "strip_orientation.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
#include "strip_orientation_data.inc"
}

static void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static uint16_t Word(std::span<const uint8_t> bytes, size_t index) {
    return uint16_t(bytes[index * 2]) | uint16_t(bytes[index * 2 + 1]) << 8;
}
static std::vector<unsigned> Stitch(std::span<const uint8_t> bytes) {
    std::vector<unsigned> out;
    bool first = true;
    for (size_t i = 0; i < bytes.size() / 2; ++i) {
        const uint16_t word = Word(bytes, i);
        const bool flip = word & 0x4000;
        bool join = false;
        if (word & 0x8000) {
            if (first) { if (!flip) first = false; }
            else { out.push_back(out.back()); join = true; }
        }
        out.push_back(word & 0x3FFF);
        if (flip && first) { first = false; out.push_back(out.back()); }
        if (join) {
            out.push_back(out.back());
            if ((out.size() % 2 == 0) != flip) out.push_back(out.back());
        }
    }
    return out;
}
int main() {
    Check(std::size(kStripRepairs) == 2564,
          "source catalog retains prior repairs and the exact VS8 outer-facade repair");
    for (const auto expected : std::array<openshao::StripRepair, 2>{{
             {0x3EEC98E922C5C2A4ull, 576, 66, 0, 6},
             {0x204C74A4D24063EBull, 576, 60, 0, 3}}}) {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), expected.signature,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == expected.signature &&
              entry->vertex_bytes == expected.vertex_bytes && entry->source_words == expected.source_words &&
              entry->offset_count == expected.offset_count,
              "VS8 sky quarters keep one-sided rasterization with consistent original strip winding");
    }
    for (const auto expected : std::array<openshao::StripRepair, 2>{{
             {0xF58F44F01B756372ull, 4880, 583, 0, 30},
             {0x658E062ECBBD2547ull, 11200, 700, 0, 179}}}) {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), expected.signature,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == expected.signature &&
              entry->vertex_bytes == expected.vertex_bytes && entry->source_words == expected.source_words &&
              entry->offset_count == expected.offset_count,
              "Survival 31 source strips use original face anchors and material-group manifold orientation");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x04E5A4E02300AC89ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x04E5A4E02300AC89ull &&
              entry->offset_count == 9,
              "Exclusive original Survival 4 geometry retains independently authored coordinate-triangle orientation");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0xBC1DDC29A6E6DAE3ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0xBC1DDC29A6E6DAE3ull &&
              entry->vertex_bytes == 76704 && entry->source_words == 7038 &&
              entry->offset_count == 1420,
              "Original Survival 31 backplate exact source restores authored Warrior Shrine submesh orientation");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0xFA7815A688F1E13Dull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0xFA7815A688F1E13Dull &&
              entry->offset_count == 81,
              "Survival 31 STONEGORO body uses independent authored 81-strip repair and preserves texture slot");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0xDA8BDC9922C0F1D8ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0xDA8BDC9922C0F1D8ull &&
              entry->vertex_bytes == 3536 && entry->source_words == 270 && entry->offset_count == 48,
              "Shokan Shrine flat floor restores 48 exact authored index flags without replacing its UVs");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x49F624809DF76469ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x49F624809DF76469ull &&
              entry->source_words == 725 && entry->offset_count == 113,
              "Survival 31 central statue restores only original Warrior Shrine-authored strip parity");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x3C9A2615CC19389Dull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x3C9A2615CC19389Dull &&
              entry->vertex_bytes == 71728 && entry->source_words == 5533 && entry->offset_count == 1137,
              "Shokan Shrine reuses independently authored geometry and restores 1137 exact strip starts");
    }
    for (uint64_t signature : {0xAFD2A1AC37DB17A7ull, 0x57AED92758B9D7BAull}) {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), signature,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == signature &&
              entry->vertex_bytes == 128 && entry->source_words == 18 && entry->offset_count == 3,
              "Vs 8 original cuboids preserve exact topology with independently proven handedness");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x885D16F58DE7E942ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x885D16F58DE7E942ull &&
              entry->vertex_bytes == 11280 && entry->source_words == 1438 && entry->offset_count == 66,
              "Vs 8 planar floor has exactly 66 independently verified upward-orientation repairs");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x68B7CF9B943101D8ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x68B7CF9B943101D8ull &&
              entry->vertex_bytes == 8880 && entry->source_words == 840 && entry->offset_count == 66,
              "Survival 3 planar floor has an exact, independently authored 66-strip correction");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x7FA7C590199F3762ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x7FA7C590199F3762ull &&
              entry->vertex_bytes == 656 && entry->source_words == 75 && entry->offset_count == 7,
              "Vs 8 authored environment strip metadata is retained as an exact content signature");
    }
    {
        const auto entry = std::lower_bound(
            std::begin(kStripRepairs), std::end(kStripRepairs), 0x17D14C6AA9D3F687ull,
            [](const openshao::StripRepair& repair, uint64_t sig) { return repair.signature < sig; });
        Check(entry != std::end(kStripRepairs) && entry->signature == 0x17D14C6AA9D3F687ull &&
              entry->vertex_bytes == 128 && entry->source_words == 18 && entry->offset_count == 3,
              "Survival 10 references preserve original XYZ/UV geometry and authored strip orientation");
    }
    for (const uint64_t signature : {0x1BC409A962CC1DECull, 0x4030C288291AA0D6ull,
                                     0x8A940C9186A9CE59ull, 0xB2845213F53620D9ull}) {
        const auto entry = std::lower_bound(
            std::begin(kTwoSidedStripSources), std::end(kTwoSidedStripSources), signature,
            [](const openshao::StripSource& source, uint64_t value) { return source.signature < value; });
        Check(entry != std::end(kTwoSidedStripSources) && entry->signature == signature &&
              entry->vertex_bytes == 768 && entry->source_words == 48,
              "Exact VS8 spear and impaled-body cards remain visible from both sides");
    }
    Check(std::size(kTwoSidedStripSources) == 483,
          "VS8 spear cards supplement the existing two-sided source identities");
    {
        const auto entry = std::lower_bound(
            std::begin(kTwoSidedStripSources), std::end(kTwoSidedStripSources), 0x55318A43D7C01FE2ull,
            [](const openshao::StripSource& source, uint64_t signature) { return source.signature < signature; });
        Check(entry != std::end(kTwoSidedStripSources) && entry->signature == 0x55318A43D7C01FE2ull &&
              entry->vertex_bytes == 6048 && entry->source_words == 387,
              "Exact VS8 raised block on the left ramp retains its exterior faces");
    }
    Check(std::size(kNoDepthWriteStripSources) == 15,
          "Exact VS8 ground and background fog layers preserve transparent overlap");
    for (size_t i = 0; i < std::size(kNoDepthWriteStripSources); ++i) {
        const auto& entry = kNoDepthWriteStripSources[i];
        Check(!i || kNoDepthWriteStripSources[i - 1].signature < entry.signature,
              "transparent source signatures are unique and sorted");
        Check((entry.vertex_bytes == 960 ||
               (entry.signature == 0x9F27E59C8C18D69Cull && entry.vertex_bytes == 640)) && entry.source_words,
              "VS8 transparent source dimensions match the original layers");
    }
    for (size_t i = 0; i < std::size(kTwoSidedStripSources); ++i) {
        const auto& entry = kTwoSidedStripSources[i];
        Check(!i || kTwoSidedStripSources[i - 1].signature < entry.signature, "two-sided signatures are unique and sorted");
        Check(entry.vertex_bytes && entry.vertex_bytes % 16 == 0 && entry.source_words,
              "two-sided source dimensions describe packed vertices and indices");
    }
    for (size_t i = 0; i < std::size(kStripRepairs); ++i) {
        const auto& repair = kStripRepairs[i];
        Check(!i || kStripRepairs[i - 1].signature < repair.signature, "content signatures are unique and sorted");
        Check(repair.vertex_bytes && repair.vertex_bytes % 16 == 0 && repair.offset_count &&
              repair.offset_begin <= std::size(kStripRepairOffsets) &&
              repair.offset_count <= std::size(kStripRepairOffsets) - repair.offset_begin,
              "catalog dimensions and complete offset ranges are valid");
        for (size_t n = 0; n < repair.offset_count; ++n) {
            auto offset = kStripRepairOffsets[repair.offset_begin + n];
            Check(offset < repair.source_words && (!n || kStripRepairOffsets[repair.offset_begin + n - 1] < offset),
                  "catalog offsets are in bounds and strictly increasing");
        }
        const auto sided = std::lower_bound(std::begin(kTwoSidedStripSources), std::end(kTwoSidedStripSources), repair.signature,
                  [](const openshao::StripSource& entry, uint64_t key) { return entry.signature < key; });
        Check(sided == std::end(kTwoSidedStripSources) || sided->signature != repair.signature,
              "orientation fixes must not also be marked two-sided");
    }
    // Two adjacent synthetic floor triangles; the second strip lacks its flip flag.
    const std::array<uint8_t, 64> vertices = {
        0,0,0,0,0,0,7,0,0,0,0,0,255,255,255,128,
        0,0,0,0,1,0,7,0,0,0,0,0,255,255,255,128,
        1,0,0,0,0,0,7,0,0,0,0,0,255,255,255,128,
        1,0,0,0,1,0,7,0,0,0,0,0,255,255,255,128};
    const std::array<uint8_t, 12> original = {0,128,1,0,2,0,1,128,2,0,3,0};
    const std::array<openshao::StripRepair, 1> catalog = {{{0xC1B702D0DAC67046ull, 64, 6, 0, 1}}};
    const std::array<uint16_t, 1> offsets = {3};
    auto source = original;
    const std::array<openshao::StripSource, 1> two_sided = {{{0xC1B702D0DAC67046ull, 64, 6}}};
    Check(openshao::HasTwoSidedStripSource(vertices, source, two_sided), "explicit two-sided source metadata is recognized");
    Check(source == original, "two-sided source classification never changes source indices");
    Check(!openshao::HasTwoSidedStripSource(vertices, source, {}), "unknown source retains its original sidedness");
    Check(openshao::RepairStripOrientation(vertices, source, catalog, offsets), "known source restores its proven orientation flag");
    const std::array<uint8_t, 12> corrected = {0,128,1,0,2,0,1,192,2,0,3,0};
    Check(source == corrected, "only the missing flag changes; vertex IDs and strip starts remain intact");
    const auto indices = Stitch(source);
    const int x[] = {0,0,1,1}, z[] = {0,1,0,1};
    unsigned up = 0, down = 0;
    for (size_t i = 0; i + 2 < indices.size(); ++i) {
        unsigned a = indices[i + (i & 1)], b = indices[i + !(i & 1)], c = indices[i + 2];
        int normal = (z[b]-z[a])*(x[c]-x[a]) - (x[b]-x[a])*(z[c]-z[a]);
        up += normal > 0; down += normal < 0;
    }
    Check(up == 2 && down == 0, "original strip assembly produces both front-facing floor triangles");
    Check(!openshao::RepairStripOrientation(vertices, source, catalog, offsets), "repair is idempotent");
    source = original; source[2] ^= 1;
    const auto unknown = source;
    Check(!openshao::RepairStripOrientation(vertices, source, catalog, offsets) && source == unknown, "modified source is preserved");
    auto changed_vertices = vertices; changed_vertices[0] = 4; source = original;
    Check(!openshao::HasTwoSidedStripSource(changed_vertices, source, two_sided), "modified object data keeps its sidedness");
    Check(!openshao::RepairStripOrientation(changed_vertices, source, catalog, offsets) && source == original, "different geometry is preserved");
    const std::array<uint16_t, 1> invalid = {6};
    source = original;
    Check(!openshao::RepairStripOrientation(vertices, source, catalog, invalid) && source == original, "invalid patch is rejected before mutation");
    const std::array<openshao::StripRepair, 1> incomplete = {{{0xC1B702D0DAC67046ull, 64, 6, 0, 2}}};
    const std::array<uint16_t, 2> partial = {3, 6};
    Check(!openshao::RepairStripOrientation(vertices, source, incomplete, partial) && source == original,
          "an invalid later offset cannot leave a partial repair");
    Check(!openshao::RepairKnownStripOrientation(vertices, source) && source == original, "unknown content is preserved by the shipping catalog");
    Check(!openshao::RepairStripOrientation(vertices, source, {}, offsets), "unknown content has no fallback rendering change");
    std::puts("PASS: exact strip repair, original assembly, unknown content, idempotence and bounds");
}
