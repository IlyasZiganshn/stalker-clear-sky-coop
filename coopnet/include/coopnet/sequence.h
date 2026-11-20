#pragma once

#include <cstdint>

namespace coopnet {

// Modular comparison on 16-bit sequences: true when `a` is newer than `b`.
// Values more than half a range apart are ambiguous; the protocol keeps windows
// well below 32768 so that never happens in practice.
inline bool sequenceNewer(std::uint16_t a, std::uint16_t b) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(a - b)) > 0;
}

}  // namespace coopnet