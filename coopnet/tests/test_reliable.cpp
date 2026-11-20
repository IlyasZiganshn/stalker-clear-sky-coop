#include "coopnet/reliable.h"

#include <cstddef>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

using coopnet::ReliableReceiver;
using coopnet::ReliableSender;

namespace {

std::vector<std::uint8_t> payloadOf(std::uint8_t value) {
    return std::vector<std::uint8_t>{value};
}

}  // namespace

COOP_TEST(reliable_sender_assigns_sequential_numbers) {
    ReliableSender sender;
    std::uint16_t seq = 0;

    CHECK(sender.send(payloadOf(1), 0, seq));
    CHECK_EQ(seq, 0);
    CHECK(sender.send(payloadOf(2), 0, seq));
    CHECK_EQ(seq, 1);
    CHECK_EQ(sender.pending(), std::size_t{2});
}

COOP_TEST(reliable_sender_retransmits_with_backoff) {
    ReliableSender sender;
    std::uint16_t seq = 0;
    CHECK(sender.send(payloadOf(1), 0, seq));

    CHECK_EQ(sender.collect(0).size(), std::size_t{1});  // first send
    CHECK_EQ(sender.collect(50).size(), std::size_t{0});  // waiting
    CHECK_EQ(sender.collect(100).size(), std::size_t{1});  // 100 ms RTO
    CHECK_EQ(sender.collect(299).size(), std::size_t{0});
    CHECK_EQ(sender.collect(300).size(), std::size_t{1});  // 200 ms RTO
    CHECK_EQ(sender.collect(699).size(), std::size_t{0});
    CHECK_EQ(sender.collect(700).size(), std::size_t{1});  // 400 ms RTO
}

COOP_TEST(reliable_sender_ack_with_holes_keeps_missing) {
    ReliableSender sender;
    std::uint16_t seq = 0;
    for (int i = 0; i < 4; ++i) {
        CHECK(sender.send(payloadOf(static_cast<std::uint8_t>(i)), 0, seq));
    }

    // ack=3, bits 0b0101: sequences 3, 2 and 0 are acknowledged, 1 is not.
    sender.onAck(3, 0b0101u);
    CHECK_EQ(sender.pending(), std::size_t{1});

    const auto resend = sender.collect(0);
    CHECK_EQ(resend.size(), std::size_t{1});
    CHECK_EQ(resend[0].sequence, 1);
}

COOP_TEST(reliable_sender_window_is_bounded) {
    ReliableSender sender;
    std::uint16_t seq = 0;
    for (std::uint16_t i = 0; i < ReliableSender::kWindow; ++i) {
        CHECK(sender.send(payloadOf(static_cast<std::uint8_t>(i)), 0, seq));
    }
    CHECK(!sender.send(payloadOf(0xFF), 0, seq));
    CHECK_EQ(sender.pending(), ReliableSender::kWindow);
}

COOP_TEST(reliable_sender_span_is_bounded) {
    ReliableSender sender;
    std::uint16_t seq = 0;
    for (std::uint16_t i = 0; i < ReliableSender::kWindow; ++i) {
        CHECK(sender.send(payloadOf(static_cast<std::uint8_t>(i)), 0, seq));
    }

    // Everything except the very first message is acknowledged: bit 30 (seq 0)
    // stays clear.
    sender.onAck(static_cast<std::uint16_t>(ReliableSender::kWindow - 1), 0x3FFFFFFFu);
    CHECK_EQ(sender.pending(), std::size_t{1});

    // One unacked message still pins the whole span: no new sends until it is
    // acknowledged, otherwise it would fall out of the ack mask.
    CHECK(!sender.send(payloadOf(1), 0, seq));
    sender.onAck(0, 0);
    CHECK(sender.send(payloadOf(1), 100, seq));
}

COOP_TEST(reliable_sender_retransmits_flags_with_the_message) {
    ReliableSender sender;
    std::uint16_t seq = 0;
    CHECK(sender.send(payloadOf(1), 0, seq, 0x80));

    const auto first = sender.collect(0);
    CHECK_EQ(first.size(), std::size_t{1});
    CHECK_EQ(first[0].flags, std::uint8_t{0x80});

    const auto again = sender.collect(100);
    CHECK_EQ(again.size(), std::size_t{1});
    CHECK_EQ(again[0].flags, std::uint8_t{0x80});
}

COOP_TEST(reliable_receiver_delivers_in_order) {
    ReliableReceiver receiver;
    CHECK(receiver.onPacket(0, 0, payloadOf(10)).size() == 1);
    CHECK(receiver.onPacket(1, 0, payloadOf(11)).size() == 1);
    CHECK_EQ(receiver.expected(), 2);
}

COOP_TEST(reliable_receiver_buffers_out_of_order) {
    ReliableReceiver receiver;

    CHECK_EQ(receiver.onPacket(2, 0, payloadOf(22)).size(), std::size_t{0});
    CHECK_EQ(receiver.onPacket(1, 0, payloadOf(11)).size(), std::size_t{0});
    CHECK_EQ(receiver.buffered(), std::size_t{2});
    CHECK_EQ(receiver.expected(), 0);

    const auto zero = receiver.onPacket(0, 0, payloadOf(10));
    CHECK_EQ(zero.size(), std::size_t{3});  // 0, then the flush of 1 and 2
    CHECK_EQ(zero[0].payload[0], 10);
    CHECK_EQ(zero[1].payload[0], 11);
    CHECK_EQ(zero[2].payload[0], 22);
    CHECK_EQ(receiver.buffered(), std::size_t{0});
    CHECK_EQ(receiver.expected(), 3);
}

COOP_TEST(reliable_receiver_carries_flags_alongside_payload) {
    ReliableReceiver receiver;

    const auto first = receiver.onPacket(0, 0x01, payloadOf(1));
    CHECK_EQ(first.size(), std::size_t{1});
    CHECK_EQ(first[0].flags, std::uint8_t{0x01});
    CHECK_EQ(first[0].payload[0], 1);

    // A buffered packet keeps its own flags when the hole is filled.
    CHECK(receiver.onPacket(2, 0x02, payloadOf(3)).empty());
    const auto flushed = receiver.onPacket(1, 0x04, payloadOf(2));
    CHECK_EQ(flushed.size(), std::size_t{2});
    CHECK_EQ(flushed[0].flags, std::uint8_t{0x04});
    CHECK_EQ(flushed[0].payload[0], 2);
    CHECK_EQ(flushed[1].flags, std::uint8_t{0x02});
    CHECK_EQ(flushed[1].payload[0], 3);
}

COOP_TEST(reliable_receiver_drops_duplicates) {
    ReliableReceiver receiver;
    CHECK_EQ(receiver.onPacket(0, 0, payloadOf(1)).size(), std::size_t{1});
    CHECK_EQ(receiver.onPacket(0, 0, payloadOf(1)).size(), std::size_t{0});
    CHECK_EQ(receiver.onPacket(0, 0, payloadOf(1)).size(), std::size_t{0});
    CHECK_EQ(receiver.expected(), 1);
}

COOP_TEST(reliable_receiver_drops_far_future_packets) {
    ReliableReceiver receiver;
    const auto tooFar = static_cast<std::uint16_t>(ReliableReceiver::kWindow + 10);
    CHECK_EQ(receiver.onPacket(tooFar, 0, payloadOf(1)).size(), std::size_t{0});
    CHECK_EQ(receiver.buffered(), std::size_t{0});
}

COOP_TEST(reliable_receiver_reports_ack_mask) {
    ReliableReceiver receiver;
    CHECK(!receiver.hasReceived());

    CHECK_EQ(receiver.onPacket(0, 0, payloadOf(1)).size(), std::size_t{1});
    CHECK(receiver.hasReceived());
    CHECK_EQ(receiver.ackSequence(), 0);
    CHECK_EQ(receiver.ackBits(), 0u);

    CHECK_EQ(receiver.onPacket(3, 0, payloadOf(4)).size(), std::size_t{0});
    CHECK_EQ(receiver.ackSequence(), 3);
    CHECK_EQ(receiver.ackBits(), 0b100u);  // 0 received, 1 and 2 missing

    CHECK_EQ(receiver.onPacket(1, 0, payloadOf(2)).size(), std::size_t{1});  // now contiguous
    CHECK_EQ(receiver.expected(), 2);
    CHECK_EQ(receiver.buffered(), std::size_t{1});  // seq 3 is still waiting for 2
    CHECK_EQ(receiver.ackBits(), 0b110u);  // 0 and 1 received, 2 still missing

    // The mask lets the sender drop everything received and keep seq 2 only.
    ReliableSender sender;
    std::uint16_t seq = 0;
    for (int i = 0; i < 4; ++i) {
        CHECK(sender.send(payloadOf(static_cast<std::uint8_t>(i)), 0, seq));
    }
    sender.onAck(receiver.ackSequence(), receiver.ackBits());
    CHECK_EQ(sender.pending(), std::size_t{1});
    const auto resend = sender.collect(0);
    CHECK_EQ(resend.size(), std::size_t{1});
    CHECK_EQ(resend[0].sequence, 2);
}

COOP_TEST(reliable_survives_lossy_link) {
    ReliableSender sender;
    ReliableReceiver receiver;
    coopnet_test::Lcg rng(0xBADC0DEu);

    constexpr std::size_t kMessages = 40;
    std::size_t nextToSend = 0;
    std::vector<std::uint8_t> delivered;

    auto pumpSends = [&](std::uint32_t now) {
        while (nextToSend < kMessages) {
            std::uint16_t seq = 0;
            if (!sender.send(payloadOf(static_cast<std::uint8_t>(nextToSend)), now,
                             seq)) {
                break;
            }
            ++nextToSend;
        }
    };

    for (std::uint32_t now = 0; now <= 20000 && delivered.size() < kMessages; ++now) {
        pumpSends(now);

        for (const auto& message : sender.collect(now)) {
            if (rng.chance(30)) {
                continue;  // lost on the wire
            }
            for (auto& received : receiver.onPacket(message.sequence, message.flags,
                                                    message.payload)) {
                delivered.push_back(received.payload[0]);
            }
        }

        if (receiver.hasReceived()) {
            sender.onAck(receiver.ackSequence(), receiver.ackBits());
        }
    }

    CHECK_EQ(delivered.size(), kMessages);
    for (std::size_t i = 0; i < delivered.size(); ++i) {
        CHECK_EQ(delivered[i], static_cast<std::uint8_t>(i));
    }
    CHECK_EQ(nextToSend, kMessages);
    CHECK_EQ(sender.pending(), std::size_t{0});
}