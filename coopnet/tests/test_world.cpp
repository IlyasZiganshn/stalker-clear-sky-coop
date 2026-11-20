#include <cstddef>
#include <cstdint>
#include <vector>

#include "coopnet/bitstream.h"
#include "coopnet/baseline.h"
#include "coopnet/delta.h"
#include "coopnet/quantize.h"
#include "coopnet/snapshot.h"
#include "coopnet/snapshot_sender.h"
#include "coopnet/snapshot_store.h"
#include "test_framework.h"
#include "test_support.h"

using coopnet::BitReader;
using coopnet::BitWriter;
using coopnet::EntityState;
using coopnet::kFieldAngles;
using coopnet::kFieldHealth;
using coopnet::kFieldPosition;
using coopnet::kFieldState;
using coopnet::Snapshot;
using coopnet::SnapshotCodec;
using coopnet::SnapshotSender;
using coopnet::SnapshotStore;
using coopnet::Vec3;

namespace {

EntityState makeEntity(std::uint32_t netId, float x, float yaw) {
    EntityState entity;
    entity.netId = netId;
    entity.classId = 1;
    entity.present = kFieldPosition | kFieldAngles | kFieldHealth | kFieldState;
    entity.position = {x, 2.0f, -3.0f};
    entity.yaw = yaw;
    entity.pitch = 0.1f;
    entity.health = 100;
    entity.state = 1;
    return entity;
}

Snapshot makeWorld(std::uint32_t tick) {
    Snapshot world;
    world.tick = tick;
    world.baseTick = 0;
    for (std::uint32_t i = 0; i < 5; ++i) {
        const float x = -80.0f + static_cast<float>(i) * 40.0f +
                        static_cast<float>(tick) * 0.1f;
        const float yaw = static_cast<float>(tick) * 0.01f +
                          static_cast<float>(i) * 0.3f;
        world.entities.push_back(makeEntity(100 + i, x, yaw));
    }
    return world;
}

bool encodeDecode(const SnapshotCodec& codec, const Snapshot& snapshot,
                  Snapshot& out) {
    BitWriter writer(256);
    if (!codec.encode(snapshot, writer)) {
        return false;
    }
    BitReader reader(writer.data().data(), writer.data().size());
    return codec.decode(reader, out);
}

bool equalEntity(const EntityState& a, const EntityState& b) {
    return a.netId == b.netId && a.present == b.present &&
           a.position.x == b.position.x && a.position.y == b.position.y &&
           a.position.z == b.position.z && a.yaw == b.yaw &&
           a.pitch == b.pitch && a.health == b.health && a.state == b.state;
}

}  // namespace

COOP_TEST(world_replication_stays_in_sync_over_a_lossy_delta_stream) {
    const SnapshotCodec codec(Vec3{-100.0f, -100.0f, -100.0f},
                              Vec3{100.0f, 100.0f, 100.0f});
    SnapshotSender sender;
    SnapshotStore store;
    coopnet_test::Lcg rng(20261120u);

    std::uint32_t delivered = 0;
    std::size_t lost = 0;
    std::size_t fullSnapshots = 0;

    for (std::uint32_t tick = 1; tick <= 300; ++tick) {
        const Snapshot world = makeWorld(tick);
        const Snapshot message = sender.next(world, store.tick());

        std::vector<std::uint8_t> datagram;
        {
            BitWriter writer(512);
            CHECK(codec.encode(message, writer));
            datagram = writer.data();
        }
        CHECK(datagram.size() <= 1200);

        if (rng.chance(10)) {
            ++lost;  // the next delta diffs against the client's actual base
            continue;
        }

        Snapshot received;
        BitReader reader(datagram.data(), datagram.size());
        CHECK(codec.decode(reader, received));

        Snapshot expected;
        CHECK(encodeDecode(codec, world, expected));
        CHECK(store.apply(received));
        CHECK_EQ(store.tick(), tick);
        CHECK_EQ(store.size(), expected.entities.size());

        for (const auto& entity : expected.entities) {
            const EntityState* actual = store.find(entity.netId);
            CHECK(actual != nullptr);
            CHECK(equalEntity(*actual, entity));
        }
        ++delivered;
        if (message.baseTick == 0) {
            ++fullSnapshots;
        }
    }

    CHECK(delivered > 200);
    CHECK(lost > 0);
    CHECK(fullSnapshots > 0);
    CHECK_EQ(store.tick(), 300u);
}