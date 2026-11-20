#include "coopnet/replicator.h"

#include <cstddef>
#include <initializer_list>

#include "test_framework.h"

using coopnet::ClientReplicator;
using coopnet::EntityState;
using coopnet::kFieldPosition;
using coopnet::Snapshot;
using coopnet::Vec3;

namespace {

EntityState makeEntity(std::uint32_t netId, float x) {
    EntityState entity;
    entity.netId = netId;
    entity.classId = 1;
    entity.present = kFieldPosition;
    entity.position = {x, 0.0f, 0.0f};
    return entity;
}

Snapshot makeWorld(std::uint32_t tick,
                   std::initializer_list<EntityState> entities) {
    Snapshot world;
    world.tick = tick;
    world.baseTick = 0;
    world.entities.assign(entities);
    return world;
}

bool contains(const Snapshot& snapshot, std::uint32_t netId) {
    for (const auto& entity : snapshot.entities) {
        if (entity.netId == netId) {
            return true;
        }
    }
    return false;
}

} // namespace

COOP_TEST(replicator_filters_per_viewer) {
    const Snapshot world =
        makeWorld(1, {makeEntity(1, 10.0f), makeEntity(2, 1000.0f)});

    ClientReplicator left;
    const Snapshot leftView = left.next(world, Vec3{0.0f, 0.0f, 0.0f}, 0);
    CHECK_EQ(leftView.entities.size(), std::size_t{1});
    CHECK(contains(leftView, 1));
    CHECK(!contains(leftView, 2));

    ClientReplicator right;
    const Snapshot rightView = right.next(world, Vec3{950.0f, 0.0f, 0.0f}, 0);
    CHECK_EQ(rightView.entities.size(), std::size_t{1});
    CHECK(contains(rightView, 2));
    CHECK(!contains(rightView, 1));
}

COOP_TEST(replicator_chains_deltas_against_the_confirmed_tick) {
    ClientReplicator replicator;
    const Vec3 viewer{};

    const Snapshot full =
        replicator.next(makeWorld(1, {makeEntity(1, 10.0f)}), viewer, 0);
    CHECK_EQ(full.baseTick, 0u);
    CHECK_EQ(full.entities.size(), std::size_t{1});

    const Snapshot delta =
        replicator.next(makeWorld(2, {makeEntity(1, 12.0f)}), viewer, 1);
    CHECK_EQ(delta.baseTick, 1u);
    CHECK_EQ(delta.entities.size(), std::size_t{1});
    CHECK_EQ(delta.entities[0].present, std::uint16_t{kFieldPosition});
    CHECK(delta.entities[0].position.x == 12.0f);
}

COOP_TEST(replicator_reports_entities_leaving_the_interest_radius) {
    ClientReplicator replicator;
    const Vec3 viewer{};

    replicator.next(
        makeWorld(1, {makeEntity(1, 10.0f), makeEntity(2, 20.0f)}), viewer, 0);

    // Entity 2 moves past keepRadius (165 m by default) and is dropped.
    const Snapshot delta = replicator.next(
        makeWorld(2, {makeEntity(1, 10.0f), makeEntity(2, 400.0f)}), viewer, 1);
    CHECK_EQ(delta.baseTick, 1u);
    CHECK(delta.entities.empty());
    CHECK_EQ(delta.removed.size(), std::size_t{1});
    CHECK_EQ(delta.removed[0], 2u);
}

COOP_TEST(replicator_reset_returns_to_full_snapshots) {
    ClientReplicator replicator;
    const Vec3 viewer{};

    replicator.next(makeWorld(1, {makeEntity(1, 10.0f)}), viewer, 0);
    replicator.reset();

    const Snapshot out =
        replicator.next(makeWorld(2, {makeEntity(1, 10.0f)}), viewer, 1);
    CHECK_EQ(out.baseTick, 0u);
    CHECK_EQ(out.entities.size(), std::size_t{1});
    CHECK_EQ(out.entities[0].present, std::uint16_t{kFieldPosition});
}