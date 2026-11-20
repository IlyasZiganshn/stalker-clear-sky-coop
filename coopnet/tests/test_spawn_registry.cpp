#include "coopnet/spawn_registry.h"

#include <cstdint>
#include <vector>

#include "test_framework.h"

namespace {

coopnet::Spawn spawn(std::uint32_t netId, std::uint8_t tag) {
    coopnet::Spawn value;
    value.netId = netId;
    value.data = {0x4D, tag};
    return value;
}

} // namespace

COOP_TEST(spawn_registry_adds_and_ignores_repeats) {
    coopnet::SpawnRegistry registry;
    CHECK(registry.observe(spawn(1001, 1)) ==
          coopnet::SpawnRegistry::Change::Added);
    CHECK_EQ(registry.size(), std::size_t{1});
    CHECK(registry.observe(spawn(1001, 1)) ==
          coopnet::SpawnRegistry::Change::Unchanged);
    CHECK_EQ(registry.size(), std::size_t{1});
    CHECK(registry.find(1001) != nullptr);
}

COOP_TEST(spawn_registry_reports_a_new_blob_for_the_same_player) {
    coopnet::SpawnRegistry registry;
    registry.observe(spawn(7, 1));
    CHECK(registry.observe(spawn(7, 2)) ==
          coopnet::SpawnRegistry::Change::Replaced);
    CHECK_EQ(registry.size(), std::size_t{1});
    const coopnet::Spawn* stored = registry.find(7);
    CHECK(stored != nullptr);
    CHECK_EQ(stored->data[1], std::uint8_t{2});
}

COOP_TEST(spawn_registry_removes_and_clears) {
    coopnet::SpawnRegistry registry;
    registry.observe(spawn(1, 1));
    registry.observe(spawn(2, 2));
    CHECK(registry.remove(1));
    CHECK(!registry.remove(1));
    CHECK(registry.find(1) == nullptr);
    CHECK_EQ(registry.size(), std::size_t{1});
    registry.clear();
    CHECK_EQ(registry.size(), std::size_t{0});
}

COOP_TEST(spawn_registry_ignores_empty_entries) {
    coopnet::SpawnRegistry registry;
    CHECK(registry.observe(coopnet::Spawn{}) ==
          coopnet::SpawnRegistry::Change::Unchanged);
    coopnet::Spawn empty = spawn(3, 1);
    empty.data.clear();
    CHECK(registry.observe(empty) == coopnet::SpawnRegistry::Change::Unchanged);
    CHECK_EQ(registry.size(), std::size_t{0});
    CHECK(registry.all().empty());
}