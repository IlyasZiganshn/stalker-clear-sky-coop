#pragma once

#include <cstdint>

#include "coopnet/baseline.h"
#include "coopnet/delta.h"
#include "coopnet/snapshot.h"

namespace coopnet {

// Server-side per-client snapshot producer: keeps the full states it sent and
// turns the current world into the next message.
// `clientBaseTick` is a snapshot the client is guaranteed to hold: the chained
// sender passes the tick it last sent, the ack-lagging path passes 0. An
// unknown base degrades to a full snapshot.
class SnapshotSender {
public:
    // `world` must be a full state (baseTick == 0) with a growing tick.
    Snapshot next(const Snapshot& world, std::uint32_t clientBaseTick);

    void clear() { bases_.clear(); }

private:
    SnapshotBaselines bases_;
};

}  // namespace coopnet