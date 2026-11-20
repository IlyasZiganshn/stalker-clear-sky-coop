#include "coopnet/snapshot.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_framework.h"

using namespace coopnet;

namespace {

SnapshotCodec makeCodec() {
    return SnapshotCodec(Vec3{-250.0f, -250.0f, 0.0f},
                         Vec3{250.0f, 250.0f, 100.0f});
}

EntityState player(std::uint32_t netId, float x, float y, float z) {
    EntityState entity;
    entity.netId = netId;
    entity.classId = 0;
    entity.present = kFieldPosition | kFieldVelocity | kFieldAngles |
                     kFieldState | kFieldHealth | kFieldWeapon |
                     kFieldAnimPhase | kFieldOwner;
    entity.position = {x, y, z};
    entity.velocity = {4.5f, -2.25f, 0.0f};
    entity.yaw = 1.234f;
    entity.pitch = -0.5f;
    entity.state = 3;
    entity.stateFlags = 0x05;
    entity.health = 87;
    entity.weaponSlot = 1;
    entity.weaponState = 2;
    entity.animPhase = 42;
    entity.ownerNetId = netId + 100;
    return entity;
}

}  // namespace

COOP_TEST(snapshot_roundtrips_entities_and_removals) {
    const SnapshotCodec codec = makeCodec();
    Snapshot original;
    original.tick = 1234;
    original.baseTick = 1200;
    original.lastProcessedInput = 740;
    original.entities.push_back(player(7, 10.5f, -20.25f, 3.0f));
    original.entities.push_back(player(9, -100.0f, 80.0f, 12.0f));
    original.removed = {5, 6};

    BitWriter writer;
    CHECK(codec.encode(original, writer));

    BitReader reader(writer.data().data(), writer.data().size());
    Snapshot decoded;
    CHECK(codec.decode(reader, decoded));
    CHECK_EQ(reader.bitPosition(), writer.bitSize());

    CHECK_EQ(decoded.tick, original.tick);
    CHECK_EQ(decoded.baseTick, original.baseTick);
    CHECK_EQ(decoded.lastProcessedInput, original.lastProcessedInput);
    CHECK_EQ(decoded.entities.size(), std::size_t{2});
    CHECK(decoded.removed == original.removed);

    const EntityState& entity = decoded.entities[0];
    CHECK_EQ(entity.netId, std::uint32_t{7});
    CHECK_EQ(entity.classId, std::uint8_t{0});
    CHECK_EQ(entity.present, original.entities[0].present);
    CHECK_NEAR(entity.position.x, 10.5f, 0.02);
    CHECK_NEAR(entity.position.y, -20.25f, 0.02);
    CHECK_NEAR(entity.position.z, 3.0f, 0.01);
    CHECK_NEAR(entity.velocity.x, 4.5f, 0.02);
    CHECK_NEAR(entity.velocity.y, -2.25f, 0.02);
    CHECK_NEAR(entity.velocity.z, 0.0f, 0.02);
    CHECK_NEAR(entity.yaw, 1.234f, 0.0002);
    CHECK_NEAR(entity.pitch, -0.5f, 0.0002);
    CHECK_EQ(entity.state, std::uint8_t{3});
    CHECK_EQ(entity.stateFlags, std::uint8_t{0x05});
    CHECK_EQ(entity.health, std::uint16_t{87});
    CHECK_EQ(entity.weaponSlot, std::uint8_t{1});
    CHECK_EQ(entity.weaponState, std::uint8_t{2});
    CHECK_EQ(entity.animPhase, std::uint8_t{42});
    CHECK_EQ(entity.ownerNetId, std::uint32_t{107});
}

COOP_TEST(snapshot_roundtrips_empty_snapshot) {
    const SnapshotCodec codec = makeCodec();
    Snapshot original;
    original.tick = 1;

    BitWriter writer;
    CHECK(codec.encode(original, writer));

    BitReader reader(writer.data().data(), writer.data().size());
    Snapshot decoded;
    CHECK(codec.decode(reader, decoded));
    CHECK_EQ(decoded.tick, std::uint32_t{1});
    CHECK_EQ(decoded.baseTick, std::uint32_t{0});
    CHECK(decoded.entities.empty());
    CHECK(decoded.removed.empty());
}

COOP_TEST(snapshot_entity_without_fields_roundtrips) {
    const SnapshotCodec codec = makeCodec();
    Snapshot original;
    EntityState bare;
    bare.netId = 42;
    bare.classId = 9;
    original.entities.push_back(bare);

    BitWriter writer;
    CHECK(codec.encode(original, writer));

    BitReader reader(writer.data().data(), writer.data().size());
    Snapshot decoded;
    CHECK(codec.decode(reader, decoded));
    CHECK_EQ(decoded.entities.size(), std::size_t{1});
    CHECK_EQ(decoded.entities[0].netId, std::uint32_t{42});
    CHECK_EQ(decoded.entities[0].classId, std::uint8_t{9});
    CHECK_EQ(decoded.entities[0].present, std::uint16_t{0});
}

COOP_TEST(snapshot_rejects_unknown_presence_bits) {
    const SnapshotCodec codec = makeCodec();
    BitWriter writer;
    writer.writeU32(1);
    writer.writeU32(0);
    writer.writeU32(0);
    writer.writeU16(1);
    writer.writeU32(1);
    writer.writeU8(0);
    writer.writeU16(1u << 15);

    BitReader reader(writer.data().data(), writer.data().size());
    Snapshot decoded;
    CHECK(!codec.decode(reader, decoded));
}

COOP_TEST(snapshot_rejects_truncated_input) {
    const SnapshotCodec codec = makeCodec();
    Snapshot original;
    original.tick = 10;
    original.entities.push_back(player(1, 1.0f, 2.0f, 3.0f));

    BitWriter writer;
    CHECK(codec.encode(original, writer));
    CHECK(writer.byteSize() > 2);

    BitReader reader(writer.data().data(), writer.byteSize() - 2);
    Snapshot decoded;
    CHECK(!codec.decode(reader, decoded));
}

COOP_TEST(snapshot_rejects_absurd_counts) {
    const SnapshotCodec codec = makeCodec();

    BitWriter entitiesWriter;
    entitiesWriter.writeU32(1);
    entitiesWriter.writeU32(0);
    entitiesWriter.writeU32(0);
    entitiesWriter.writeU16(
        static_cast<std::uint16_t>(SnapshotCodec::kMaxEntities + 1));
    BitReader entitiesReader(entitiesWriter.data().data(),
                             entitiesWriter.data().size());
    Snapshot decoded;
    CHECK(!codec.decode(entitiesReader, decoded));

    BitWriter removedWriter;
    removedWriter.writeU32(1);
    removedWriter.writeU32(0);
    removedWriter.writeU32(0);
    removedWriter.writeU16(0);
    removedWriter.writeU32(
        static_cast<std::uint32_t>(SnapshotCodec::kMaxEntities + 1));
    BitReader removedReader(removedWriter.data().data(),
                            removedWriter.data().size());
    CHECK(!codec.decode(removedReader, decoded));
}

COOP_TEST(snapshot_refuses_to_encode_absurd_counts) {
    const SnapshotCodec codec = makeCodec();
    Snapshot original;
    original.removed.assign(SnapshotCodec::kMaxEntities + 1, 1);
    BitWriter writer;
    CHECK(!codec.encode(original, writer));
}

COOP_TEST(snapshot_player_record_fits_a_budget) {
    const SnapshotCodec codec = makeCodec();
    Snapshot original;
    original.entities.push_back(player(1, 0.0f, 0.0f, 0.0f));

    BitWriter writer;
    CHECK(codec.encode(original, writer));
    // A full player record plus the snapshot header must stay well under one
    // MTU; 20 Hz snapshots of a whole level depend on this.
    CHECK(writer.byteSize() < 64);
}