#pragma once
#include <cstdint>
#include <span>

namespace openshao {
struct StripRepair {
    uint64_t signature;
    uint32_t vertex_bytes, source_words, offset_begin, offset_count;
};
struct StripSource {
    uint64_t signature;
    uint32_t vertex_bytes, source_words;
};
bool HasTwoSidedStripSource(std::span<const uint8_t> vertices, std::span<const uint8_t> source,
                           std::span<const StripSource> sources);
bool KnownStripSourceNeedsTwoSides(std::span<const uint8_t> vertices, std::span<const uint8_t> source);
bool KnownRoomStripSourceNeedsTwoSides(uint32_t room, std::span<const uint8_t> vertices,
                                     std::span<const uint8_t> source);
bool KnownStripSourceNeedsNoDepthWrite(std::span<const uint8_t> vertices, std::span<const uint8_t> source);
bool KnownRoomStripSourceNeedsNoDepthWrite(uint32_t room,std::span<const uint8_t> vertices,
                                         std::span<const uint8_t> source);
inline constexpr unsigned BackgroundStripSourceCount = 4;
// IDs identify the four original VS8 sky pieces; zero leaves content unchanged.
unsigned KnownBackgroundStripSourceId(std::span<const uint8_t> vertices, std::span<const uint8_t> source);
// Restore authored A9 soul-layer state before the native scene parser consumes it.
bool RestoreKnownArenaSoulMaterials(uint32_t room, std::span<uint8_t> scene);
// Restore proven house opacity before parsing the original AA or BB textures.
bool RestoreKnownWastelandHouseOpacity(uint32_t room, std::span<uint8_t> archive);
bool RepairStripOrientation(std::span<const uint8_t> vertices, std::span<uint8_t> source,
                           std::span<const StripRepair> repairs, std::span<const uint16_t> offsets);
bool RepairKnownStripOrientation(std::span<const uint8_t> vertices, std::span<uint8_t> source);
bool RepairKnownRoomStripOrientation(uint32_t room, std::span<const uint8_t> vertices,
                                    std::span<uint8_t> source);
}
