#pragma once

#include <cstdint>

#include "coopnet/interest.h"
#include "coopnet/snapshot_sender.h"

namespace coopnet {

// Per-client server-side replication: keeps only the entities this viewer can
// see and chains deltas from the snapshot the client confirmed. One instance
// per connected client; baselines are never shared between clients.
class ClientReplicator {
public:
    explicit ClientReplicator(InterestConfig interest = {});

    // `world` must be a full server state with a growing tick. `clientBaseTick`
    // is the snapshot tick the client holds (0 = none). The result is a full
    // snapshot or a delta against that base.
    Snapshot next(const Snapshot& world, const Vec3& viewer,
                  std::uint32_t clientBaseTick);

    // Drops interest hysteresis and delta history; the next message is full.
    void reset();

private:
    InterestFilter interest_;
    SnapshotSender sender_;
};

} // namespace coopnet