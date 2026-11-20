#include "coopnet/ids.h"

#include <cstddef>

#include "test_framework.h"

using coopnet::NetIdTable;

COOP_TEST(net_ids_are_stable_per_object) {
    NetIdTable ids;
    const std::uint32_t first = ids.assign(7);
    CHECK(first != NetIdTable::kInvalidNetId);
    CHECK_EQ(ids.assign(7), first);
    CHECK_EQ(ids.size(), std::size_t{1});

    const std::uint32_t second = ids.assign(3);
    CHECK(second != first);
    CHECK_EQ(ids.find(3), second);
    CHECK_EQ(ids.find(7), first);
    CHECK_EQ(ids.find(9), NetIdTable::kInvalidNetId);
}

COOP_TEST(net_ids_look_up_both_ways) {
    NetIdTable ids;
    const std::uint32_t netId = ids.assign(42);

    std::uint16_t objectId = 0;
    CHECK(ids.objectId(netId, objectId));
    CHECK_EQ(objectId, std::uint16_t{42});

    std::uint16_t missing = 0;
    CHECK(!ids.objectId(netId + 100, missing));
}

COOP_TEST(net_ids_erase_and_never_reuse) {
    NetIdTable ids;
    const std::uint32_t first = ids.assign(1);
    const std::uint32_t second = ids.assign(2);

    CHECK(ids.erase(first));
    CHECK(!ids.erase(first));
    CHECK_EQ(ids.find(1), NetIdTable::kInvalidNetId);
    CHECK_EQ(ids.size(), std::size_t{1});

    const std::uint32_t third = ids.assign(3);
    CHECK(third != first);
    CHECK(third != second);
    CHECK_EQ(ids.issued(), third);

    CHECK(ids.eraseObject(2));
    CHECK_EQ(ids.find(2), NetIdTable::kInvalidNetId);
    CHECK(!ids.eraseObject(2));
}

COOP_TEST(net_ids_survive_a_level_change) {
    NetIdTable ids;
    const std::uint32_t beforeLevel = ids.assign(5);
    ids.clear();
    CHECK_EQ(ids.size(), std::size_t{0});
    CHECK_EQ(ids.find(5), NetIdTable::kInvalidNetId);

    // Object ids restart on every level, net ids must not.
    const std::uint32_t afterLevel = ids.assign(5);
    CHECK(afterLevel != beforeLevel);
    CHECK(afterLevel > beforeLevel);
    CHECK_EQ(ids.issued(), afterLevel);
}