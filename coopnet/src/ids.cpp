#include "coopnet/ids.h"

namespace coopnet {

std::uint32_t NetIdTable::assign(std::uint16_t objectId) {
    const auto it = byObject_.find(objectId);
    if (it != byObject_.end()) {
        return it->second;
    }
    const std::uint32_t netId = next_++;
    byObject_.emplace(objectId, netId);
    byNet_.emplace(netId, objectId);
    return netId;
}

std::uint32_t NetIdTable::find(std::uint16_t objectId) const {
    const auto it = byObject_.find(objectId);
    return it == byObject_.end() ? kInvalidNetId : it->second;
}

bool NetIdTable::objectId(std::uint32_t netId, std::uint16_t& out) const {
    const auto it = byNet_.find(netId);
    if (it == byNet_.end()) {
        return false;
    }
    out = it->second;
    return true;
}

bool NetIdTable::erase(std::uint32_t netId) {
    const auto it = byNet_.find(netId);
    if (it == byNet_.end()) {
        return false;
    }
    byObject_.erase(it->second);
    byNet_.erase(it);
    return true;
}

bool NetIdTable::eraseObject(std::uint16_t objectId) {
    const auto it = byObject_.find(objectId);
    if (it == byObject_.end()) {
        return false;
    }
    byNet_.erase(it->second);
    byObject_.erase(it);
    return true;
}

void NetIdTable::clear() {
    byObject_.clear();
    byNet_.clear();
}

}  // namespace coopnet