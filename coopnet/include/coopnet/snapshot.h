#pragma once

#include <cstdint>
#include <vector>

#include "coopnet/bitstream.h"
#include "coopnet/quantize.h"

namespace coopnet {

// Presence bits of an entity record. The order is fixed by docs/PROTOCOL.md.
enum EntityField : std::uint16_t {
    kFieldPosition = 1u << 0,
    kFieldVelocity = 1u << 1,
    kFieldAngles = 1u << 2,
    kFieldState = 1u << 3,
    kFieldHealth = 1u << 4,
    kFieldWeapon = 1u << 5,
    kFieldAnimPhase = 1u << 6,
    kFieldAiState = 1u << 7,
    kFieldOwner = 1u << 8,
};

constexpr std::uint16_t kKnownFields = (1u << 9) - 1u;

struct EntityState {
    std::uint32_t netId = 0;
    std::uint8_t classId = 0;
    std::uint16_t present = 0;

    Vec3 position;
    Vec3 velocity;
    float yaw = 0.0f;
    float pitch = 0.0f;
    std::uint8_t state = 0;
    std::uint8_t stateFlags = 0;
    std::uint16_t health = 0;
    std::uint8_t weaponSlot = 0;
    std::uint8_t weaponState = 0;
    std::uint8_t animPhase = 0;
    std::uint8_t aiState = 0;
    std::uint32_t ownerNetId = 0;
};

struct Snapshot {
    std::uint32_t tick = 0;
    std::uint32_t baseTick = 0;  // 0 = full snapshot
    std::vector<EntityState> entities;
    std::vector<std::uint32_t> removed;
};

// Entity records are quantized against level bounds; both peers must use the
// same bounds (announced when the level loads). Decoding is strict: unknown
// presence bits, impossible counts and truncation are errors.
class SnapshotCodec {
public:
    static constexpr std::size_t kMaxEntities = 8192;

    SnapshotCodec(const Vec3& levelMin, const Vec3& levelMax);

    bool encode(const Snapshot& snapshot, BitWriter& writer) const;
    bool decode(BitReader& reader, Snapshot& out) const;

private:
    void encodeEntity(const EntityState& entity, BitWriter& writer) const;
    bool decodeEntity(BitReader& reader, EntityState& out) const;

    PositionCodec positions_;
};

}  // namespace coopnet