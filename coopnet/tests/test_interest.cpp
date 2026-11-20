#include "coopnet/interest.h"

#include <cstddef>
#include <initializer_list>
#include <vector>

#include "test_framework.h"

using coopnet::EntityState;
using coopnet::InterestConfig;
using coopnet::InterestFilter;
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

COOP_TEST(interest_keeps_entities_inside_the_radius) {
    InterestFilter filter(InterestConfig{100.0f, 110.0f});
    const Snapshot world =
        makeWorld(7, {makeEntity(1, 10.0f), makeEntity(2, 150.0f)});

    const Snapshot out = filter.filter(world, Vec3{});
    CHECK_EQ(out.tick, 7u);
    CHECK_EQ(out.baseTick, 0u);
    CHECK_EQ(out.entities.size(), std::size_t{1});
    CHECK_EQ(out.entities[0].netId, 1u);
    CHECK_EQ(filter.visibleCount(), std::size_t{1});
}

COOP_TEST(interest_hysteresis_holds_the_boundary) {
    InterestFilter filter(InterestConfig{100.0f, 110.0f});
    const Vec3 viewer{};

    const Snapshot first = filter.filter(makeWorld(1, {makeEntity(1, 95.0f)}), viewer);
    CHECK_EQ(first.entities.size(), std::size_t{1});

    // 105 m is outside radius but inside keepRadius: still sent.
    const Snapshot kept = filter.filter(makeWorld(2, {makeEntity(1, 105.0f)}), viewer);
    CHECK_EQ(kept.entities.size(), std::size_t{1});

    // 120 m is outside keepRadius: dropped.
    const Snapshot dropped = filter.filter(makeWorld(3, {makeEntity(1, 120.0f)}), viewer);
    CHECK(dropped.entities.empty());
    CHECK_EQ(filter.visibleCount(), std::size_t{0});

    // After the drop, 105 m is evaluated against radius: not visible again.
    const Snapshot back = filter.filter(makeWorld(4, {makeEntity(1, 105.0f)}), viewer);
    CHECK(back.entities.empty());
}

COOP_TEST(interest_keeps_entities_without_position) {
    EntityState marker = makeEntity(2, 500.0f);
    marker.present = 0;

    InterestFilter filter(InterestConfig{100.0f, 110.0f});
    const Snapshot out = filter.filter(makeWorld(1, {marker}), Vec3{});
    CHECK_EQ(out.entities.size(), std::size_t{1});
    CHECK_EQ(out.entities[0].netId, 2u);
}

COOP_TEST(interest_reset_drops_hysteresis_state) {
    InterestFilter filter(InterestConfig{100.0f, 110.0f});
    const Vec3 viewer{};

    filter.filter(makeWorld(1, {makeEntity(1, 95.0f)}), viewer);
    filter.reset();
    CHECK_EQ(filter.visibleCount(), std::size_t{0});

    const Snapshot out = filter.filter(makeWorld(2, {makeEntity(1, 105.0f)}), viewer);
    CHECK(out.entities.empty());
}

COOP_TEST(interest_forgets_entities_missing_from_the_world) {
    InterestFilter filter(InterestConfig{100.0f, 110.0f});
    const Vec3 viewer{};

    filter.filter(makeWorld(1, {makeEntity(1, 95.0f)}), viewer);
    const Snapshot empty = filter.filter(makeWorld(2, {}), viewer);
    CHECK(empty.entities.empty());
    CHECK_EQ(filter.visibleCount(), std::size_t{0});

    // The hysteresis state does not resurrect the entity.
    const Snapshot out = filter.filter(makeWorld(3, {makeEntity(1, 105.0f)}), viewer);
    CHECK(out.entities.empty());
}