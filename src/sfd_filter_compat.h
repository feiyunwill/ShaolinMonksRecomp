#pragma once
#include <atomic>
#include <cstdint>

namespace openshao::sfd {

// Apply reconstruction filtering only while the native CRI soft-display filter is enabled.
inline std::atomic<bool> soft_display_filter{false};

inline void SetSoftDisplayFilter(uint32_t enable) {
    soft_display_filter.store(enable != 0, std::memory_order_relaxed);
}

inline uint32_t PresentationTaps() {
    return soft_display_filter.load(std::memory_order_relaxed) ? 2u : 1u;
}

}
