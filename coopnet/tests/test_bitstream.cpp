#include "coopnet/bitstream.h"

#include <cstring>

#include "test_framework.h"

using coopnet::BitReader;
using coopnet::BitWriter;

COOP_TEST(bitstream_mixed_widths_roundtrip) {
    BitWriter writer;
    writer.writeBits(0x3u, 2);
    writer.writeBits(0x1FFFFFu, 21);
    writer.writeU32(0xDEADBEEFu);
    writer.writeBits(0x15u, 5);
    writer.writeBits(0u, 0);  // zero-width writes are ignored
    writer.writeBits(0xFFFFFFFFu, 7);  // masked to 0x7F

    CHECK_EQ(writer.bitSize(), std::size_t{2 + 21 + 32 + 5 + 7});
    CHECK_EQ(writer.byteSize(), (writer.bitSize() + 7) / 8);

    BitReader reader(writer.data().data(), writer.data().size());
    CHECK_EQ(reader.readBits(2), 0x3u);
    CHECK_EQ(reader.readBits(21), 0x1FFFFFu);
    CHECK_EQ(reader.readU32(), 0xDEADBEEFu);
    CHECK_EQ(reader.readBits(5), 0x15u);
    CHECK_EQ(reader.readBits(7), 0x7Fu);
    CHECK(reader.ok());
    CHECK_EQ(reader.bitPosition(), writer.bitSize());
}

COOP_TEST(bitstream_alignment_and_bytes) {
    BitWriter writer;
    writer.writeBits(1u, 1);
    writer.alignToByte();
    writer.writeU8(0xAB);
    writer.writeBytes("\x01\x02\x03", 3);

    CHECK_EQ(writer.byteSize(), std::size_t{5});
    CHECK_EQ(writer.data()[0], 0x01);
    CHECK_EQ(writer.data()[1], 0xAB);

    BitReader reader(writer.data().data(), writer.data().size());
    CHECK_EQ(reader.readBits(1), 1u);
    reader.alignToByte();
    CHECK_EQ(reader.readU8(), 0xAB);

    std::uint8_t tail[3] = {};
    CHECK(reader.readBytes(tail, 3));
    CHECK_EQ(tail[0], 1);
    CHECK_EQ(tail[1], 2);
    CHECK_EQ(tail[2], 3);
    CHECK(reader.ok());
}

COOP_TEST(bitstream_varuint_boundaries) {
    const std::uint32_t values[] = {0u,      1u,       7u,       127u,
                                    128u,    16383u,   16384u,   2097151u,
                                    2097152u, 0xFFFFFFFFu};
    BitWriter writer;
    for (const std::uint32_t value : values) {
        writer.writeVarUInt(value);
    }

    BitReader reader(writer.data().data(), writer.data().size());
    for (const std::uint32_t value : values) {
        CHECK_EQ(reader.readVarUInt(), value);
    }
    CHECK(reader.ok());
}

COOP_TEST(bitstream_signed_roundtrip) {
    const std::int32_t values[] = {0, 1, -1, 2, -2, 63, -64, 2147483647,
                                   -2147483647 - 1};
    BitWriter writer;
    for (const std::int32_t value : values) {
        writer.writeSigned(value);
    }

    BitReader reader(writer.data().data(), writer.data().size());
    for (const std::int32_t value : values) {
        CHECK_EQ(reader.readSigned(), value);
    }
    CHECK(reader.ok());
}

COOP_TEST(bitstream_float_is_bit_exact) {
    const float values[] = {0.0f, -0.0f, 1.0f, -1.5f, 3.14159265f, 1e30f, 1e-30f};
    BitWriter writer;
    for (const float value : values) {
        writer.writeFloat(value);
    }

    BitReader reader(writer.data().data(), writer.data().size());
    for (const float value : values) {
        std::uint32_t expected_bits = 0;
        std::memcpy(&expected_bits, &value, sizeof(expected_bits));

        const float got = reader.readFloat();
        std::uint32_t got_bits = 0;
        std::memcpy(&got_bits, &got, sizeof(got_bits));

        CHECK_EQ(got_bits, expected_bits);
    }
    CHECK(reader.ok());
}

COOP_TEST(bitstream_overrun_is_detected) {
    const std::uint8_t data[] = {0xFF};
    BitReader reader(data, sizeof(data));

    CHECK_EQ(reader.readBits(4), 0xFu);
    CHECK(reader.ok());
    CHECK_EQ(reader.bitsLeft(), std::size_t{4});

    CHECK_EQ(reader.readBits(5), 0u);
    CHECK(!reader.ok());
    CHECK_EQ(reader.readBits(1), 0u);  // stays failed, keeps returning zeros
}

COOP_TEST(bitstream_truncated_varuint_is_detected) {
    const std::uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x80};
    BitReader reader(data, sizeof(data));
    CHECK_EQ(reader.readVarUInt(), 0u);
    CHECK(!reader.ok());
}

COOP_TEST(bitstream_bytes_out_of_bounds) {
    const std::uint8_t data[] = {0x00, 0x00};
    BitReader reader(data, sizeof(data));
    std::uint8_t out[4] = {};
    CHECK(!reader.readBytes(out, 4));
    CHECK(!reader.ok());
}

COOP_TEST(bitstream_clear_resets_state) {
    BitWriter writer;
    writer.writeU32(0x12345678u);
    CHECK_EQ(writer.bitSize(), std::size_t{32});
    writer.clear();
    CHECK_EQ(writer.bitSize(), std::size_t{0});
    CHECK(writer.data().empty());
    writer.writeU8(0x7F);
    CHECK_EQ(writer.data()[0], 0x7F);
}