#include "coopnet/snapshot.h"

namespace coopnet {

SnapshotCodec::SnapshotCodec(const Vec3& levelMin, const Vec3& levelMax)
    : positions_(levelMin, levelMax) {}

bool SnapshotCodec::encode(const Snapshot& snapshot, BitWriter& writer) const {
    if (snapshot.entities.size() > kMaxEntities ||
        snapshot.removed.size() > kMaxEntities) {
        return false;
    }
    writer.writeU32(snapshot.tick);
    writer.writeU32(snapshot.baseTick);
    writer.writeU16(static_cast<std::uint16_t>(snapshot.entities.size()));
    for (const auto& entity : snapshot.entities) {
        encodeEntity(entity, writer);
    }
    writer.writeU32(static_cast<std::uint32_t>(snapshot.removed.size()));
    for (const auto netId : snapshot.removed) {
        writer.writeU32(netId);
    }
    return true;
}

bool SnapshotCodec::decode(BitReader& reader, Snapshot& out) const {
    Snapshot snapshot;
    snapshot.tick = reader.readU32();
    snapshot.baseTick = reader.readU32();
    const std::uint16_t entityCount = reader.readU16();
    if (!reader.ok() || entityCount > kMaxEntities) {
        return false;
    }
    snapshot.entities.reserve(entityCount);
    for (std::uint16_t i = 0; i < entityCount; ++i) {
        EntityState entity;
        if (!decodeEntity(reader, entity)) {
            return false;
        }
        snapshot.entities.push_back(entity);
    }

    const std::uint32_t removedCount = reader.readU32();
    if (!reader.ok() || removedCount > kMaxEntities) {
        return false;
    }
    snapshot.removed.reserve(removedCount);
    for (std::uint32_t i = 0; i < removedCount; ++i) {
        snapshot.removed.push_back(reader.readU32());
    }
    if (!reader.ok()) {
        return false;
    }
    out = std::move(snapshot);
    return true;
}

void SnapshotCodec::encodeEntity(const EntityState& entity,
                                 BitWriter& writer) const {
    writer.writeU32(entity.netId);
    writer.writeU8(entity.classId);
    writer.writeU16(entity.present);

    if (entity.present & kFieldPosition) {
        std::uint32_t qx = 0;
        std::uint32_t qy = 0;
        std::uint32_t qz = 0;
        positions_.encode(entity.position, qx, qy, qz);
        const int bits = positions_.bitsPerComponent();
        writer.writeBits(qx, bits);
        writer.writeBits(qy, bits);
        writer.writeBits(qz, bits);
    }
    if (entity.present & kFieldVelocity) {
        writer.writeBits(encodeVelocity(entity.velocity.x), 12);
        writer.writeBits(encodeVelocity(entity.velocity.y), 12);
        writer.writeBits(encodeVelocity(entity.velocity.z), 12);
    }
    if (entity.present & kFieldAngles) {
        writer.writeU16(encodeAngle(entity.yaw));
        writer.writeU16(encodePitch(entity.pitch));
    }
    if (entity.present & kFieldState) {
        writer.writeU8(entity.state);
        writer.writeU8(entity.stateFlags);
    }
    if (entity.present & kFieldHealth) {
        writer.writeU16(entity.health);
    }
    if (entity.present & kFieldWeapon) {
        writer.writeU8(entity.weaponSlot);
        writer.writeU8(entity.weaponState);
    }
    if (entity.present & kFieldAnimPhase) {
        writer.writeU8(entity.animPhase);
    }
    if (entity.present & kFieldAiState) {
        writer.writeU8(entity.aiState);
    }
    if (entity.present & kFieldOwner) {
        writer.writeU32(entity.ownerNetId);
    }
}

bool SnapshotCodec::decodeEntity(BitReader& reader, EntityState& out) const {
    EntityState entity;
    entity.netId = reader.readU32();
    entity.classId = reader.readU8();
    entity.present = reader.readU16();
    if (!reader.ok() || (entity.present & ~kKnownFields) != 0) {
        return false;
    }

    if (entity.present & kFieldPosition) {
        const int bits = positions_.bitsPerComponent();
        const std::uint32_t qx = reader.readBits(bits);
        const std::uint32_t qy = reader.readBits(bits);
        const std::uint32_t qz = reader.readBits(bits);
        entity.position = positions_.decode(qx, qy, qz);
    }
    if (entity.present & kFieldVelocity) {
        entity.velocity.x = decodeVelocity(static_cast<std::uint16_t>(reader.readBits(12)));
        entity.velocity.y = decodeVelocity(static_cast<std::uint16_t>(reader.readBits(12)));
        entity.velocity.z = decodeVelocity(static_cast<std::uint16_t>(reader.readBits(12)));
    }
    if (entity.present & kFieldAngles) {
        entity.yaw = decodeAngle(reader.readU16());
        entity.pitch = decodePitch(reader.readU16());
    }
    if (entity.present & kFieldState) {
        entity.state = reader.readU8();
        entity.stateFlags = reader.readU8();
    }
    if (entity.present & kFieldHealth) {
        entity.health = reader.readU16();
    }
    if (entity.present & kFieldWeapon) {
        entity.weaponSlot = reader.readU8();
        entity.weaponState = reader.readU8();
    }
    if (entity.present & kFieldAnimPhase) {
        entity.animPhase = reader.readU8();
    }
    if (entity.present & kFieldAiState) {
        entity.aiState = reader.readU8();
    }
    if (entity.present & kFieldOwner) {
        entity.ownerNetId = reader.readU32();
    }
    if (!reader.ok()) {
        return false;
    }
    out = entity;
    return true;
}

}  // namespace coopnet