#pragma once

#include <cstdint>

#include "coopnet/baseline.h"
#include "coopnet/delta.h"
#include "coopnet/snapshot.h"

namespace coopnet {

// Server-side per-client snapshot producer: keeps the full states it sent and
// turns the current world into the next message. A delta is built only against
// the tick the client confirmed; anything else degrades to a full snapshot.
class SnapshotSender {
public:
    // `world` must be a full state (baseTick == 0) with a growing tick.
    // `ackedTick` is the last snapshot the client confirmed; 0 means none.
    Snapshot next(const Snapshot& world, std::uint32_t ackedTick);

    void clear() { bases_.clear(); }

private:
    SnapshotBaselines bases_;
};

}  // namespace coopnet