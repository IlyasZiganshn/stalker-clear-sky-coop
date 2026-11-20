#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coopnet {

// LSB-first bit stream. Snapshot records are packed without byte padding, so a
// couple of dozen entities fit into one MTU-sized datagram.
class BitWriter {
public:
    explicit BitWriter(std::size_t reserveBytes = 0);

    // bits in [0, 32]; values wider than `bits` are masked, not rejected
    void writeBits(std::uint32_t value, int bits);
    void writeBool(bool value) { writeBits(value ? 1u : 0u, 1); }
    void writeU8(std::uint8_t value) { writeBits(value, 8); }
    void writeU16(std::uint16_t value) { writeBits(value, 16); }
    void writeU32(std::uint32_t value) { writeBits(value, 32); }
    void writeVarUInt(std::uint32_t value);
    void writeSigned(std::int32_t value);  // zigzag + varuint
    void writeFloat(float value);
    void writeBytes(const void* data, std::size_t size);
    void alignToByte();

    void clear();

    const std::vector<std::uint8_t>& data() const { return bytes_; }
    std::size_t bitSize() const { return bitPos_; }
    std::size_t byteSize() const { return (bitPos_ + 7) / 8; }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t bitPos_ = 0;
};

// Every read is bounds-checked: on overrun the stream latches `ok()` to false
// and returns zeros, so callers validate once at the end of a decode.
class BitReader {
public:
    BitReader(const void* data, std::size_t size);

    std::uint32_t readBits(int bits);
    bool readBool() { return readBits(1) != 0; }
    std::uint8_t readU8() { return static_cast<std::uint8_t>(readBits(8)); }
    std::uint16_t readU16() { return static_cast<std::uint16_t>(readBits(16)); }
    std::uint32_t readU32() { return readBits(32); }
    std::uint32_t readVarUInt();
    std::int32_t readSigned();
    float readFloat();
    bool readBytes(void* out, std::size_t size);
    bool skipBits(std::size_t count);
    void alignToByte();

    bool ok() const { return ok_; }
    std::size_t bitPosition() const { return bitPos_; }
    std::size_t bitsLeft() const { return bitPos_ < sizeBits_ ? sizeBits_ - bitPos_ : 0; }

private:
    bool canRead(std::size_t bits) const {
        return ok_ && bits <= sizeBits_ - bitPos_;
    }

    const std::uint8_t* data_ = nullptr;
    std::size_t sizeBits_ = 0;
    std::size_t bitPos_ = 0;
    bool ok_ = true;
};

}  // namespace coopnet