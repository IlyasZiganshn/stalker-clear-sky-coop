#include "coopnet/snapshot_sender.h"

namespace coopnet {

Snapshot SnapshotSender::next(const Snapshot& world,
                              std::uint32_t clientBaseTick) {
    Snapshot message;
    const Snapshot* base =
        clientBaseTick != 0 ? bases_.find(clientBaseTick) : nullptr;
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