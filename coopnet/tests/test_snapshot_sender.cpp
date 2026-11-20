#include "coopnet/snapshot_sender.h"

#include <cstddef>
#include <vector>

#include "test_framework.h"

using coopnet::EntityState;
using coopnet::kFieldHealth;
using coopnet::kFieldPosition;
using coopnet::Snapshot;
using coopnet::SnapshotSender;

namespace {

EntityState makeEntity(std::uint32_t netId, float x) {
    EntityState entity;
    entity.netId = netId;
    entity.classId = 1;
    entity.present = kFieldPosition | kFieldHealth;
    entity.position = {x, 0.0f, 0.0f};
    entity.health = 100;
    return entity;
}

Snapshot makeWorld(std::uint32_t tick, float x) {
    Snapshot world;
    world.tick = tick;
    world.baseTick = 0;
    world.entities.push_back(makeEntity(1, x));
    return world;
}

}  // namespace

COOP_TEST(snapshot_sender_starts_with_a_full_snapshot) {
    SnapshotSender sender;
    const Snapshot world = makeWorld(1, 1.0f);

    const Snapshot message = sender.next(world, 0);
    CHECK_EQ(message.tick, 1u);
    CHECK_EQ(message.baseTick, 0u);
    CHECK_EQ(message.entities.size(), std::size_t{1});
    CHECK_EQ(message.entities[0].present, world.entities[0].present);
}

COOP_TEST(snapshot_sender_diffs_against_the_acknowledged_tick) {
    SnapshotSender sender;
    sender.next(makeWorld(1, 1.0f), 0);

    const Snapshot message = sender.next(makeWorld(2, 3.0f), 1);
    CHECK_EQ(message.tick, 2u);
    CHECK_EQ(message.baseTick, 1u);
    CHECK_EQ(message.entities.size(), std::size_t{1});
    CHECK_EQ(message.entities[0].present, std::uint16_t{kFieldPosition});
    CHECK(message.entities[0].position.x == 3.0f);

    // Nothing moved: an empty delta is correct.
    const Snapshot idle = sender.next(makeWorld(3, 3.0f), 2);
    CHECK_EQ(idle.baseTick, 2u);
    CHECK(idle.entities.empty());
}

COOP_TEST(snapshot_sender_falls_back_to_full_on_unknown_base) {
    SnapshotSender sender;
    sender.next(makeWorld(1, 1.0f), 0);

    const Snapshot message = sender.next(makeWorld(2, 2.0f), 99);
    CHECK_EQ(message.baseTick, 0u);
    CHECK_EQ(message.entities.size(), std::size_t{1});

    // After a full snapshot the new tick becomes a valid base again.
    const Snapshot delta = sender.next(makeWorld(3, 4.0f), 2);
    CHECK_EQ(delta.baseTick, 2u);
    CHECK_EQ(delta.entities[0].present, std::uint16_t{kFieldPosition});
}