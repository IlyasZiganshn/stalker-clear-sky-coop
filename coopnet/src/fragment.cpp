#include "coopnet/fragment.h"

#include <algorithm>
#include <limits>

namespace coopnet {
namespace {

void putU16(std::uint8_t* out, std::uint16_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

void putU32(std::uint8_t* out, std::uint32_t value) {
    out[0] = static_cast<std::uint8_t>(value & 0xFFu);
    out[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    out[2] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    out[3] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
}

std::uint16_t getU16(const std::uint8_t* in) {
    return static_cast<std::uint16_t>(in[0] | (in[1] << 8));
}

std::uint32_t getU32(const std::uint8_t* in) {
    return static_cast<std::uint32_t>(in[0]) |
           (static_cast<std::uint32_t>(in[1]) << 8) |
           (static_cast<std::uint32_t>(in[2]) << 16) |
           (static_cast<std::uint32_t>(in[3]) << 24);
}

}  // namespace

std::vector<std::uint8_t> FragmentHeader::encode() const {
    std::vector<std::uint8_t> out(kByteSize);
    putU32(out.data(), messageId);
    putU16(out.data() + 4, index);
    putU16(out.data() + 6, count);
    return out;
}

bool FragmentHeader::decode(const std::uint8_t* data, std::size_t size,
                            FragmentHeader& out) {
    if (size < kByteSize) {
        return false;
    }
    out.messageId = getU32(data);
    out.index = getU16(data + 4);
    out.count = getU16(data + 6);
    return true;
}

std::vector<std::vector<std::uint8_t>> fragmentMessage(std::uint32_t messageId,
                                                       const std::uint8_t* data,
                                                       std::size_t size,
                                                       std::size_t maxDatagram) {
    std::vector<std::vector<std::uint8_t>> out;
    if (maxDatagram <= FragmentHeader::kByteSize) {
        return out;
    }
    const std::size_t chunk = maxDatagram - FragmentHeader::kByteSize;
    const std::size_t count = size == 0 ? 1 : (size + chunk - 1) / chunk;
    if (count > std::numeric_limits<std::uint16_t>::max()) {
        return out;
    }

    if (size == 0) {
        FragmentHeader header;
        header.messageId = messageId;
        header.index = 0;
        header.count = 1;
        out.push_back(header.encode());
        return out;
    }

    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = i * chunk;
        const std::size_t length = std::min(chunk, size - offset);

        FragmentHeader header;
        header.messageId = messageId;
        header.index = static_cast<std::uint16_t>(i);
        header.count = static_cast<std::uint16_t>(count);

        std::vector<std::uint8_t> datagram = header.encode();
        datagram.insert(datagram.end(), data + offset, data + offset + length);
        out.push_back(std::move(datagram));
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> Reassembler::onFragment(
    const FragmentHeader& header, const std::uint8_t* payload, std::size_t size,
    std::uint32_t nowMs) {
    if (header.count == 0 || header.index >= header.count) {
        return std::nullopt;
    }

    auto it = partials_.find(header.messageId);
    if (it == partials_.end()) {
        Partial partial;
        partial.count = header.count;
        partial.received = 0;
        partial.lastUpdateMs = nowMs;
        partial.chunks.resize(header.count);
        partial.have.assign(header.count, false);
        it = partials_.emplace(header.messageId, std::move(partial)).first;
    }

    Partial& partial = it->second;
    if (partial.count != header.count) {
        return std::nullopt;  // inconsistent metadata, drop and let the sender retry
    }

    partial.lastUpdateMs = nowMs;
    if (partial.have[header.index]) {
        return std::nullopt;  // duplicate fragment
    }

    partial.chunks[header.index].assign(payload, payload + size);
    partial.have[header.index] = true;
    ++partial.received;

    if (partial.received < partial.count) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> message;
    for (const auto& chunk : partial.chunks) {
        message.insert(message.end(), chunk.begin(), chunk.end());
    }

    std::optional<std::vector<std::uint8_t>> result = std::move(message);
    partials_.erase(it);
    return result;
}

void Reassembler::expire(std::uint32_t nowMs) {
    for (auto it = partials_.begin(); it != partials_.end();) {
        if (nowMs - it->second.lastUpdateMs >= timeoutMs_) {
            it = partials_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace coopnet