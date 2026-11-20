#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace coopnet {

// Session-wide mapping between the engine's per-level object id (u16, reused
// after a level change) and the wire's net id (u32, monotonic for the whole
// session). Clearing on a level change drops the table but keeps the counter:
// ids from the previous level never get reused on the wire.
class NetIdTable {
public:
    static constexpr std::uint32_t kInvalidNetId = 0;

    // Returns the existing net id, or assigns the next one.
    std::uint32_t assign(std::uint16_t objectId);

    std::uint32_t find(std::uint16_t objectId) const;

    // False when the net id is unknown.
    bool objectId(std::uint32_t netId, std::uint16_t& out) const;

    // Drops only the mapping; the net id is never handed out again.
    bool erase(std::uint32_t netId);

    bool eraseObject(std::uint16_t objectId);

    void clear();

    std::size_t size() const { return byObject_.size(); }
    std::uint32_t issued() const { return next_ - 1; }

private:
    std::map<std::uint16_t, std::uint32_t> byObject_;
    std::map<std::uint32_t, std::uint16_t> byNet_;
    std::uint32_t next_ = 1;
};

}  // namespace coopnet