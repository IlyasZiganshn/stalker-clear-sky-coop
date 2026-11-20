#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coopnet_test {

// Deterministic generator: no dependence on <random> implementation details.
class Lcg {
public:
    explicit Lcg(std::uint32_t seed = 0x12345678u) : state_(seed) {}

    std::uint32_t next() {
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }

    float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }

    // True with the given probability in percent.
    bool chance(int percent) { return next() % 100u < static_cast<std::uint32_t>(percent); }

private:
    std::uint32_t state_;
};

inline std::vector<std::uint8_t> makeBytes(std::size_t size, std::uint32_t seed) {
    std::vector<std::uint8_t> bytes(size);
    Lcg rng(seed);
    for (auto& byte : bytes) {
        byte = static_cast<std::uint8_t>(rng.next() >> 24);
    }
    return bytes;
}

inline std::vector<std::uint8_t> makeRange(std::size_t size, std::uint8_t first = 0) {
    std::vector<std::uint8_t> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::uint8_t>(first + i);
    }
    return bytes;
}

}  // namespace coopnet_test