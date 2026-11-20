#include "coopnet/bitstream.h"

#include <cstring>

namespace coopnet {
namespace {

constexpr int kMaxBitsPerValue = 32;

std::uint32_t zigzag(std::int32_t value) {
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    const std::uint32_t sign = value < 0 ? 0xFFFFFFFFu : 0u;
    return (bits << 1) ^ sign;
}

std::int32_t unzigzag(std::uint32_t value) {
    return static_cast<std::int32_t>((value >> 1) ^ (0u - (value & 1u)));
}

}  // namespace

BitWriter::BitWriter(std::size_t reserveBytes) {
    bytes_.reserve(reserveBytes);
}

void BitWriter::writeBits(std::uint32_t value, int bits) {
    if (bits <= 0) {
        return;
    }
    if (bits > kMaxBitsPerValue) {
        bits = kMaxBitsPerValue;
    }
    if (bits < kMaxBitsPerValue) {
        value &= (1u << bits) - 1u;
    }

    const std::size_t endBit = bitPos_ + static_cast<std::size_t>(bits);
    if ((endBit + 7) / 8 > bytes_.size()) {
        bytes_.resize((endBit + 7) / 8, 0);
    }

    for (int i = 0; i < bits; ++i) {
        if ((value >> i) & 1u) {
            const std::size_t pos = bitPos_ + static_cast<std::size_t>(i);
            bytes_[pos >> 3] |= static_cast<std::uint8_t>(1u << (pos & 7u));
        }
    }
    bitPos_ = endBit;
}

void BitWriter::writeVarUInt(std::uint32_t value) {
    while (value >= 0x80u) {
        writeU8(static_cast<std::uint8_t>(value) | 0x80u);
        value >>= 7;
    }
    writeU8(static_cast<std::uint8_t>(value));
}

void BitWriter::writeSigned(std::int32_t value) {
    writeVarUInt(zigzag(value));
}

void BitWriter::writeFloat(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32 bit");
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(bits);
}

void BitWriter::writeBytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        writeU8(bytes[i]);
    }
}

void BitWriter::alignToByte() {
    bitPos_ = (bitPos_ + 7u) & ~std::size_t{7u};
    const std::size_t required = byteSize();
    if (bytes_.size() < required) {
        bytes_.resize(required, 0);
    }
}

void BitWriter::clear() {
    bytes_.clear();
    bitPos_ = 0;
}

BitReader::BitReader(const void* data, std::size_t size)
    : data_(static_cast<const std::uint8_t*>(data)), sizeBits_(size * 8) {}

std::uint32_t BitReader::readBits(int bits) {
    if (bits <= 0) {
        return 0;
    }
    if (bits > kMaxBitsPerValue) {
        bits = kMaxBitsPerValue;
    }
    if (!canRead(static_cast<std::size_t>(bits))) {
        ok_ = false;
        return 0;
    }

    std::uint32_t value = 0;
    for (int i = 0; i < bits; ++i) {
        const std::size_t pos = bitPos_ + static_cast<std::size_t>(i);
        value |= static_cast<std::uint32_t>((data_[pos >> 3] >> (pos & 7u)) & 1u) << i;
    }
    bitPos_ += static_cast<std::size_t>(bits);
    return value;
}

std::uint32_t BitReader::readVarUInt() {
    std::uint32_t result = 0;
    for (int shift = 0; shift < 32; shift += 7) {
        if (!canRead(8)) {
            ok_ = false;
            return 0;
        }
        const std::uint32_t byte = readBits(8);
        result |= (byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) {
            return result;
        }
    }
    // Five continuation bytes: not a valid 32-bit varint.
    ok_ = false;
    return 0;
}

std::int32_t BitReader::readSigned() {
    return unzigzag(readVarUInt());
}

float BitReader::readFloat() {
    const std::uint32_t bits = readU32();
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool BitReader::readBytes(void* out, std::size_t size) {
    auto* bytes = static_cast<std::uint8_t*>(out);
    for (std::size_t i = 0; i < size; ++i) {
        if (!canRead(8)) {
            ok_ = false;
            return false;
        }
        bytes[i] = readU8();
    }
    return true;
}

bool BitReader::skipBits(std::size_t count) {
    if (!canRead(count)) {
        ok_ = false;
        return false;
    }
    bitPos_ += count;
    return true;
}

void BitReader::alignToByte() {
    const std::size_t aligned = (bitPos_ + 7u) & ~std::size_t{7u};
    if (aligned > sizeBits_) {
        ok_ = false;
        bitPos_ = sizeBits_;
        return;
    }
    bitPos_ = aligned;
}

}  // namespace coopnet