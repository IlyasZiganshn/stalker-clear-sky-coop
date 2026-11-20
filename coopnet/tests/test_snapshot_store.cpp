#include "coopnet/snapshot_store.h"

#include <cstddef>
#include <cstdint>

#include "test_framework.h"

using namespace coopnet;

namespace {

EntityState makeEntity(std::uint32_t netId, float x, float y, float z,
                       std::uint16_t health) {
    EntityState entity;
    entity.netId = netId;
    entity.present = kFieldPosition | kFieldHealth;
    entity.position = {x, y, z};
    entity.health = health;
    return entity;
}

Snapshot fullSnapshot(std::uint32_t tick) {
    Snapshot snapshot;
    snapshot.tick = tick;
    snapshot.baseTick = 0;
    snapshot.entities.push_back(makeEntity(1, 1.0f, 2.0f, 3.0f, 100));
    snapshot.entities.push_back(makeEntity(2, -1.0f, -2.0f, -3.0f, 50));
    return snapshot;
}

}  // namespace

COOP_TEST(store_applies_full_snapshot) {
    SnapshotStore store;
    CHECK(store.apply(fullSnapshot(10)));

    CHECK_EQ(store.tick(), std::uint32_t{10});
    CHECK_EQ(store.size(), std::size_t{2});
    const EntityState* first = store.find(1);
    CHECK(first != nullptr);
    if (first != nullptr) {
        CHECK(first->position.x == 1.0f);
        CHECK_EQ(first->health, std::uint16_t{100});
    }
    CHECK(store.find(3) == nullptr);

    // A second full snapshot replaces the world instead of merging.
    Snapshot replacement;
    replacement.tick = 20;
    replacement.entities.push_back(makeEntity(3, 0.0f, 0.0f, 0.0f, 10));
    CHECK(store.apply(replacement));
    CHECK_EQ(store.tick(), std::uint32_t{20});
    CHECK_EQ(store.size(), std::size_t{1});
    CHECK(store.find(1) == nullptr);
}

COOP_TEST(store_merges_delta_fields) {
    SnapshotStore store;
    CHECK(store.apply(fullSnapshot(10)));

    Snapshot delta;
    delta.tick = 11;
    delta.baseTick = 10;
    EntityState moved;
    moved.netId = 1;
    moved.present = kFieldPosition;
    moved.position = {9.0f, 8.0f, 7.0f};
    // Garbage in fields that are not present must be ignored.
    moved.health = 1;
    delta.entities.push_back(moved);

    CHECK(store.apply(delta));
    CHECK_EQ(store.tick(), std::uint32_t{11});

    const EntityState* entity = store.find(1);
    CHECK(entity != nullptr);
    if (entity != nullptr) {
        CHECK(entity->position.x == 9.0f);
        CHECK(entity->position.y == 8.0f);
        CHECK_EQ(entity->health, std::uint16_t{100});  // untouched
    }
    CHECK_EQ(store.size(), std::size_t{2});
}

COOP_TEST(store_rejects_delta_with_wrong_base) {
    SnapshotStore store;
    CHECK(store.apply(fullSnapshot(10)));

    Snapshot stale;
    stale.tick = 11;
    stale.baseTick = 9;
    stale.entities.push_back(makeEntity(1, 100.0f, 0.0f, 0.0f, 1));

    CHECK(!store.apply(stale));
    CHECK_EQ(store.tick(), std::uint32_t{10});
    const EntityState* entity = store.find(1);
    CHECK(entity != nullptr);
    if (entity != nullptr) {
        CHECK(entity->position.x == 1.0f);
        CHECK_EQ(entity->health, std::uint16_t{100});
    }
}

COOP_TEST(store_applies_removals_and_spawns) {
    SnapshotStore store;
    CHECK(store.apply(fullSnapshot(10)));

    Snapshot delta;
    delta.tick = 11;
    delta.baseTick = 10;
    delta.removed = {2};
    delta.entities.push_back(makeEntity(3, 5.0f, 5.0f, 5.0f, 25));

    CHECK(store.apply(delta));
    CHECK_EQ(store.size(), std::size_t{2});
    CHECK(store.find(2) == nullptr);
    CHECK(store.find(3) != nullptr);
}

COOP_TEST(store_keeps_last_processed_input) {
    SnapshotStore store;
    Snapshot full = fullSnapshot(10);
    full.lastProcessedInput = 123;
    CHECK(store.apply(full));
    CHECK_EQ(store.lastProcessedInput(), std::uint32_t{123});

    Snapshot delta;
    delta.tick = 11;
    delta.baseTick = 10;
    delta.lastProcessedInput = 130;
    CHECK(store.apply(delta));
    CHECK_EQ(store.lastProcessedInput(), std::uint32_t{130});

    store.clear();
    CHECK_EQ(store.lastProcessedInput(), std::uint32_t{0});
}

COOP_TEST(store_clear_resets_everything) {
    SnapshotStore store;
    CHECK(store.apply(fullSnapshot(10)));
    store.clear();
    CHECK(store.empty());
    CHECK_EQ(store.tick(), std::uint32_t{0});
    CHECK(store.find(1) == nullptr);
}