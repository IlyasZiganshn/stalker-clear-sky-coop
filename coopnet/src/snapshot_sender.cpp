#include "coopnet/snapshot_sender.h"

namespace coopnet {

Snapshot SnapshotSender::next(const Snapshot& world, std::uint32_t ackedTick) {
    Snapshot message;
    const Snapshot* base = ackedTick != 0 ? bases_.find(ackedTick) : nullptr;
    if (base != nullptr) {
        message = SnapshotDelta::makeDelta(*base, world);
    } else {
        message = world;
        message.baseTick = 0;
    }

    Snapshot full = world;
    full.baseTick = 0;
    bases_.record(std::move(full));
    return message;
}

}  // namespace coopnet