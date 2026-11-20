#include "coopnet/replicator.h"

namespace coopnet {

ClientReplicator::ClientReplicator(InterestConfig interest)
    : interest_(interest) {}

Snapshot ClientReplicator::next(const Snapshot& world, const Vec3& viewer,
                                std::uint32_t clientBaseTick) {
    return sender_.next(interest_.filter(world, viewer), clientBaseTick);
}

void ClientReplicator::reset() {
    interest_.reset();
    sender_.clear();
}

} // namespace coopnet