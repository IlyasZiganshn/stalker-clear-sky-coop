#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace coopnet {

// 8-byte fragment header stored little-endian in front of each chunk.
struct FragmentHeader {
    std::uint32_t messageId = 0;
    std::uint16_t index = 0;
    std::uint16_t count = 0;

    static constexpr std::size_t kByteSize = 8;

    std::vector<std::uint8_t> encode() const;
    static bool decode(const std::uint8_t* data, std::size_t size, FragmentHeader& out);
};

// Splits a message into datagrams of at most `maxDatagram` bytes, header
// included. Returns empty when the message cannot be represented (too many
// fragments) or maxDatagram is smaller than the fragment header.
std::vector<std::vector<std::uint8_t>> fragmentMessage(std::uint32_t messageId,
                                                       const std::uint8_t* data,
                                                       std::size_t size,
                                                       std::size_t maxDatagram);

class Reassembler {
public:
    static constexpr std::uint32_t kDefaultTimeoutMs = 5000;

    Reassembler() : timeoutMs_(kDefaultTimeoutMs) {}
    explicit Reassembler(std::uint32_t timeoutMs) : timeoutMs_(timeoutMs) {}

    // Returns the whole message once the last missing fragment arrives.
    std::optional<std::vector<std::uint8_t>> onFragment(const FragmentHeader& header,
                                                        const std::uint8_t* payload,
                                                        std::size_t size,
                                                        std::uint32_t nowMs);

    void expire(std::uint32_t nowMs);
    std::size_t pendingMessages() const { return partials_.size(); }

private:
    struct Partial {
        std::uint16_t count = 0;
        std::uint16_t received = 0;
        std::uint32_t lastUpdateMs = 0;
        std::vector<std::vector<std::uint8_t>> chunks;
        std::vector<bool> have;
    };

    std::map<std::uint32_t, Partial> partials_;
    std::uint32_t timeoutMs_;
};

}  // namespace coopnet