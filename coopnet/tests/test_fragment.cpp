#include "coopnet/fragment.h"

#include <cstddef>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

using coopnet::FragmentHeader;
using coopnet::Reassembler;

namespace {

constexpr std::size_t kMtu = 1200;

std::optional<std::vector<std::uint8_t>> feed(
    Reassembler& reassembler, const std::vector<std::uint8_t>& datagram,
    std::uint32_t nowMs = 0) {
    FragmentHeader header;
    if (!FragmentHeader::decode(datagram.data(), datagram.size(), header)) {
        return std::nullopt;
    }
    return reassembler.onFragment(header, datagram.data() + FragmentHeader::kByteSize,
                                  datagram.size() - FragmentHeader::kByteSize, nowMs);
}

}  // namespace

COOP_TEST(fragment_header_roundtrip) {
    FragmentHeader header;
    header.messageId = 0xCAFEBABEu;
    header.index = 3;
    header.count = 7;

    const auto bytes = header.encode();
    CHECK_EQ(bytes.size(), FragmentHeader::kByteSize);

    FragmentHeader decoded;
    CHECK(FragmentHeader::decode(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.messageId, header.messageId);
    CHECK_EQ(decoded.index, header.index);
    CHECK_EQ(decoded.count, header.count);

    CHECK(!FragmentHeader::decode(bytes.data(), bytes.size() - 1, decoded));
}

COOP_TEST(fragment_splits_and_reassembles) {
    const auto message = coopnet_test::makeBytes(5000, 7);
    const auto datagrams = coopnet::fragmentMessage(42, message.data(), message.size(), kMtu);

    CHECK_EQ(datagrams.size(), std::size_t{5});
    for (const auto& datagram : datagrams) {
        CHECK(datagram.size() <= kMtu);
    }

    Reassembler reassembler;
    std::optional<std::vector<std::uint8_t>> result;
    for (const auto& datagram : datagrams) {
        result = feed(reassembler, datagram);
    }

    CHECK(result.has_value());
    CHECK(result->size() == message.size());
    CHECK(*result == message);
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{0});
}

COOP_TEST(fragment_reassembles_out_of_order) {
    const auto message = coopnet_test::makeBytes(3000, 11);
    auto datagrams = coopnet::fragmentMessage(7, message.data(), message.size(), kMtu);
    CHECK_EQ(datagrams.size(), std::size_t{3});

    Reassembler reassembler;
    CHECK(!feed(reassembler, datagrams[2]).has_value());
    CHECK(!feed(reassembler, datagrams[0]).has_value());
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{1});

    const auto result = feed(reassembler, datagrams[1]);
    CHECK(result.has_value());
    CHECK(*result == message);
}

COOP_TEST(fragment_duplicates_are_ignored) {
    const auto message = coopnet_test::makeBytes(1500, 3);
    auto datagrams = coopnet::fragmentMessage(9, message.data(), message.size(), kMtu);
    CHECK_EQ(datagrams.size(), std::size_t{2});

    Reassembler reassembler;
    CHECK(!feed(reassembler, datagrams[0]).has_value());
    CHECK(!feed(reassembler, datagrams[0]).has_value());
    const auto result = feed(reassembler, datagrams[1]);
    CHECK(result.has_value());
    CHECK(*result == message);
}

COOP_TEST(fragment_inconsistent_metadata_is_rejected) {
    const auto message = coopnet_test::makeBytes(2500, 5);
    auto datagrams = coopnet::fragmentMessage(1, message.data(), message.size(), kMtu);

    FragmentHeader header;
    CHECK(FragmentHeader::decode(datagrams[0].data(), datagrams[0].size(), header));

    Reassembler reassembler;
    CHECK(!feed(reassembler, datagrams[0]).has_value());

    // Same message id, different fragment count: must not be mixed in.
    FragmentHeader lie = header;
    lie.index = 1;
    lie.count = static_cast<std::uint16_t>(header.count + 1);
    const std::uint8_t stub[4] = {1, 2, 3, 4};
    CHECK(!reassembler.onFragment(lie, stub, sizeof(stub), 10).has_value());
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{1});
}

COOP_TEST(fragment_invalid_index_is_rejected) {
    Reassembler reassembler;
    FragmentHeader header;
    header.messageId = 1;
    header.index = 4;
    header.count = 3;  // index outside of count

    const std::uint8_t stub[4] = {1, 2, 3, 4};
    CHECK(!reassembler.onFragment(header, stub, sizeof(stub), 0).has_value());
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{0});
}

COOP_TEST(fragment_empty_message_roundtrip) {
    const auto datagrams = coopnet::fragmentMessage(3, nullptr, 0, kMtu);
    CHECK_EQ(datagrams.size(), std::size_t{1});

    Reassembler reassembler;
    const auto result = feed(reassembler, datagrams[0]);
    CHECK(result.has_value());
    CHECK(result->empty());
}

COOP_TEST(fragment_rejects_too_small_datagram) {
    const std::uint8_t data[8] = {};
    CHECK(coopnet::fragmentMessage(1, data, sizeof(data), FragmentHeader::kByteSize).empty());
}

COOP_TEST(fragment_exact_multiple_has_no_trailing_fragment) {
    const std::size_t chunk = kMtu - FragmentHeader::kByteSize;
    const auto message = coopnet_test::makeRange(chunk * 2, 1);
    const auto datagrams = coopnet::fragmentMessage(5, message.data(), message.size(), kMtu);
    CHECK_EQ(datagrams.size(), std::size_t{2});
}

COOP_TEST(fragment_incomplete_message_expires) {
    const auto message = coopnet_test::makeBytes(3000, 13);
    auto datagrams = coopnet::fragmentMessage(77, message.data(), message.size(), kMtu);

    Reassembler reassembler(1000);
    CHECK(!feed(reassembler, datagrams[0]).has_value());
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{1});

    reassembler.expire(999);
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{1});

    reassembler.expire(1000);
    CHECK_EQ(reassembler.pendingMessages(), std::size_t{0});
}