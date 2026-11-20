#include "coopnet/delta.h"
#include "coopnet/snapshot_store.h"

#include <cstddef>
#include <vector>

#include "test_framework.h"

using coopnet::EntityState;
using coopnet::kFieldAiState;
using coopnet::kFieldAngles;
using coopnet::kFieldHealth;
using coopnet::kFieldPosition;
using coopnet::kFieldState;
using coopnet::Snapshot;
using coopnet::SnapshotDelta;
using coopnet::SnapshotStore;

namespace {

EntityState makeEntity(std::uint32_t netId, float x, std::uint8_t state = 0) {
    EntityState entity;
    entity.netId = netId;
    entity.classId = 1;
    entity.present = kFieldPosition | kFieldState | kFieldHealth;
    entity.position = {x, 0.0f, 0.0f};
    entity.state = state;
    entity.health = 100;
    return entity;
}

Snapshot makeSnapshot(std::uint32_t tick, std::uint32_t baseTick,
                      std::vector<EntityState> entities) {
    Snapshot snapshot;
    snapshot.tick = tick;
    snapshot.baseTick = baseTick;
    snapshot.entities = std::move(entities);
    return snapshot;
}

}  // namespace

COOP_TEST(delta_skips_unchanged_entities) {
    const Snapshot base = makeSnapshot(10, 0, {makeEntity(1, 1.0f)});
    const Snapshot current = makeSnapshot(11, 10, {makeEntity(1, 1.0f)});

    const Snapshot delta = SnapshotDelta::makeDelta(base, current);
    CHECK_EQ(delta.tick, 11u);
    CHECK_EQ(delta.baseTick, 10u);
    CHECK(delta.entities.empty());
    CHECK(delta.removed.empty());
}

COOP_TEST(delta_carries_only_changed_fields) {
    const Snapshot base = makeSnapshot(10, 0, {makeEntity(1, 1.0f)});
    const Snapshot current = makeSnapshot(11, 10, {makeEntity(1, 5.0f)});

    const Snapshot delta = SnapshotDelta::makeDelta(base, current);
    CHECK_EQ(delta.entities.size(), std::size_t{1});
    CHECK_EQ(delta.entities[0].netId, 1u);
    CHECK_EQ(delta.entities[0].classId, std::uint8_t{1});
    CHECK_EQ(delta.entities[0].present, std::uint16_t{kFieldPosition});
    CHECK(delta.entities[0].position.x == 5.0f);

    EntityState changedState = makeEntity(1, 1.0f, 3);
    const Snapshot current2 = makeSnapshot(11, 10, {changedState});
    const Snapshot delta2 = SnapshotDelta::makeDelta(base, current2);
    CHECK_EQ(delta2.entities.size(), std::size_t{1});
    CHECK_EQ(delta2.entities[0].present, std::uint16_t{kFieldState});
    CHECK_EQ(delta2.entities[0].state, std::uint8_t{3});
}

COOP_TEST(delta_sends_new_entities_in_full_and_lists_removals) {
    const Snapshot base = makeSnapshot(10, 0, {makeEntity(1, 1.0f)});
    const Snapshot current = makeSnapshot(11, 10, {makeEntity(2, 2.0f)});

    const Snapshot delta = SnapshotDelta::makeDelta(base, current);
    CHECK_EQ(delta.entities.size(), std::size_t{1});
    CHECK_EQ(delta.entities[0].netId, 2u);
    CHECK_EQ(delta.entities[0].present,
             std::uint16_t{kFieldPosition | kFieldState | kFieldHealth});
    CHECK_EQ(delta.removed.size(), std::size_t{1});
    CHECK_EQ(delta.removed[0], 1u);
}

COOP_TEST(delta_from_empty_base_is_a_full_snapshot) {
    Snapshot base;
    base.tick = 0;
    const Snapshot current = makeSnapshot(1, 0, {makeEntity(1, 1.0f)});

    const Snapshot delta = SnapshotDelta::makeDelta(base, current);
    CHECK_EQ(delta.baseTick, 0u);
    CHECK_EQ(delta.entities.size(), std::size_t{1});
    CHECK_EQ(delta.entities[0].present, current.entities[0].present);
}

COOP_TEST(delta_round_trips_through_the_store) {
    Snapshot base = makeSnapshot(10, 0, {makeEntity(1, 1.0f), makeEntity(2, 2.0f)});
    Snapshot current = makeSnapshot(11, 10, {makeEntity(1, 4.0f, 1), makeEntity(3, 3.0f)});

    SnapshotStore store;
    CHECK(store.apply(base));
    CHECK(store.apply(SnapshotDelta::makeDelta(base, current)));
    CHECK_EQ(store.tick(), 11u);
    CHECK_EQ(store.size(), std::size_t{2});

    const EntityState* first = store.find(1);
    CHECK(first != nullptr);
    CHECK(first->position.x == 4.0f);
    CHECK_EQ(first->state, std::uint8_t{1});

    CHECK(store.find(2) == nullptr);
    CHECK(store.find(3) != nullptr);
}

COOP_TEST(delta_marks_a_field_that_appeared_in_the_current_mask) {
    EntityState baseEntity = makeEntity(1, 1.0f);
    baseEntity.present = kFieldPosition;
    EntityState currentEntity = makeEntity(1, 1.0f);
    currentEntity.present = kFieldPosition | kFieldAngles;
    currentEntity.yaw = 1.0f;

    const Snapshot base = makeSnapshot(10, 0, {baseEntity});
    const Snapshot current = makeSnapshot(11, 10, {currentEntity});

    const Snapshot delta = SnapshotDelta::makeDelta(base, current);
    CHECK_EQ(delta.entities.size(), std::size_t{1});
    CHECK_EQ(delta.entities[0].present, std::uint16_t{kFieldAngles});
    CHECK(delta.entities[0].yaw == 1.0f);
}