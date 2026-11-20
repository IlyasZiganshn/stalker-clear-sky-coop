#include "coopnet/session.h"

#include <algorithm>
#include <utility>

namespace coopnet {

Session::Session(SessionConfig config) : config_(config) {
    channels_[static_cast<std::size_t>(Channel::Control)].reliable = true;
    channels_[static_cast<std::size_t>(Channel::Events)].reliable = true;
}

Session::ChannelState& Session::state(Channel channel) {
    return channels_[static_cast<std::size_t>(channel)];
}

const Session::ChannelState& Session::state(Channel channel) const {
    return channels_[static_cast<std::size_t>(channel)];
}

void Session::emit(std::vector<std::vector<std::uint8_t>>& out, Channel channel,
                   ChannelState& channelState, std::uint16_t sequence,
                   const std::vector<std::uint8_t>& payload, std::uint8_t flags) {
    PacketHeader header;
    header.protocol = config_.protocol;
    header.channel = channel;
    header.flags = flags;
    header.sequence = sequence;
    // On reliable channels the receiver's ack state is authoritative: it only
    // records sequences that were actually accepted, so a datagram outside the
    // receive window never confirms itself.
    if (channelState.reliable) {
        if (channelState.receiver.hasReceived()) {
            header.ack = channelState.receiver.ackSequence();
            header.ackBits = channelState.receiver.ackBits();
        }
    } else if (channelState.ack.hasReceived()) {
        header.ack = channelState.ack.ackSequence();
        header.ackBits = channelState.ack.ackBits();
    }

    BitWriter writer(PacketHeader::kByteSize + payload.size());
    header.write(writer);
    if (!payload.empty()) {
        writer.writeBytes(payload.data(), payload.size());
    }

    out.push_back(writer.data());
    ++stats_.sentDatagrams;
    if ((flags & kFlagAckOnly) == 0) {
        channelState.lastSentSequence = sequence;
    }
}

void Session::onDatagram(const std::uint8_t* data, std::size_t size,
                         std::uint32_t nowMs) {
    ++stats_.receivedDatagrams;
    if (size < PacketHeader::kByteSize) {
        ++stats_.malformed;
        return;
    }

    BitReader reader(data, size);
    PacketHeader header;
    if (!PacketHeader::read(reader, header)) {
        ++stats_.malformed;
        return;
    }
    if (header.protocol != config_.protocol) {
        ++stats_.protocolErrors;
        return;
    }
    if (static_cast<std::size_t>(header.channel) >= kChannelCount) {
        ++stats_.malformed;
        return;
    }

    ChannelState& channelState = state(header.channel);

    if ((header.flags & kFlagAckOnly) != 0) {
        // Pure acknowledgement: never acknowledge an acknowledgement, or two
        // peers ping-pong ack-only datagrams forever.
        if (channelState.reliable) {
            channelState.sender.onAck(header.ack, header.ackBits);
        }
        return;
    }

    if (channelState.reliable) {
        channelState.sender.onAck(header.ack, header.ackBits);
    } else {
        channelState.ack.record(header.sequence);
    }
    channelState.ackDirty = true;

    std::vector<std::uint8_t> payload(size - PacketHeader::kByteSize);
    if (!payload.empty() &&
        !reader.readBytes(payload.data(), payload.size())) {
        ++stats_.malformed;
        return;
    }
    if (payload.empty()) {
        ++stats_.malformed;
        return;
    }

    if (channelState.reliable) {
        // Fragments travel as ordinary ordered messages; reassembly is fed from
        // the delivered stream, never from raw packet arrival order.
        for (auto& message : channelState.receiver.onPacket(
                 header.sequence, header.flags, std::move(payload))) {
            if ((message.flags & kFlagFragment) == 0) {
                delivered_.push_back({header.channel, std::move(message.payload)});
                continue;
            }
            FragmentHeader fragment;
            if (!FragmentHeader::decode(message.payload.data(),
                                        message.payload.size(), fragment)) {
                ++stats_.malformed;
                continue;
            }
            const std::size_t fragmentPayload =
                message.payload.size() - FragmentHeader::kByteSize;
            auto complete = channelState.reassembler.onFragment(
                fragment, message.payload.data() + FragmentHeader::kByteSize,
                fragmentPayload, nowMs);
            // Fragments of one message are enqueued back to back, so the last
            // one completes the message at its proper place in the stream.
            if (complete) {
                delivered_.push_back({header.channel, std::move(*complete)});
            }
        }
        return;
    }

    // Unreliable channels are sequenced, not ordered: a partly received
    // fragmented message could never be reassembled, so the sender never
    // fragments here.
    if ((header.flags & kFlagFragment) != 0) {
        ++stats_.malformed;
        return;
    }

    // Unreliable sequenced: newer snapshots replace older ones, stale ones are
    // dropped instead of delivered late.
    if (channelState.hasUnreliableReceived &&
        !sequenceNewer(header.sequence, channelState.lastUnreliableReceived)) {
        ++stats_.droppedDatagrams;
        return;
    }
    channelState.hasUnreliableReceived = true;
    channelState.lastUnreliableReceived = header.sequence;
    delivered_.push_back({header.channel, std::move(payload)});
}

std::vector<std::vector<std::uint8_t>> Session::poll(std::uint32_t nowMs) {
    std::vector<std::vector<std::uint8_t>> out;

    for (std::size_t i = 0; i < kChannelCount; ++i) {
        auto& channelState = channels_[i];
        const auto channel = static_cast<Channel>(i);

        if (channelState.reliable) {
            for (const auto& message : channelState.sender.collect(nowMs)) {
                emit(out, channel, channelState, message.sequence, message.payload,
                     static_cast<std::uint8_t>(kFlagReliable | message.flags));
            }

            while (!channelState.outbox.empty()) {
                std::uint16_t sequence = 0;
                if (!channelState.sender.send(channelState.outbox.front().payload,
                                              nowMs, sequence,
                                              channelState.outbox.front().flags)) {
                    break;
                }
                const std::uint8_t flags =
                    static_cast<std::uint8_t>(kFlagReliable |
                                              channelState.outbox.front().flags);
                std::vector<std::uint8_t> payload =
                    std::move(channelState.outbox.front().payload);
                channelState.outbox.pop_front();
                emit(out, channel, channelState, sequence, payload, flags);
            }
        } else {
            while (!channelState.outbox.empty()) {
                const std::uint8_t flags = channelState.outbox.front().flags;
                std::vector<std::uint8_t> payload =
                    std::move(channelState.outbox.front().payload);
                channelState.outbox.pop_front();
                emit(out, channel, channelState,
                     channelState.nextUnreliableSend++, payload, flags);
            }
        }

        if (channelState.ackDirty) {
            emit(out, channel, channelState, channelState.lastSentSequence, {},
                 kFlagAckOnly);
            channelState.ackDirty = false;
        }

        channelState.reassembler.expire(nowMs);
    }

    return out;
}

bool Session::send(Channel channel, std::vector<std::uint8_t> payload) {
    auto& channelState = state(channel);
    if (config_.mtu <= PacketHeader::kByteSize) {
        return false;
    }
    const std::size_t budget = config_.mtu - PacketHeader::kByteSize;

    if (payload.size() <= budget) {
        if (channelState.outbox.size() >= config_.outboxLimit) {
            return false;
        }
        channelState.outbox.push_back({std::move(payload), 0});
        return true;
    }

    if (!channelState.reliable) {
        // Unreliable channels cannot reassemble what arrives out of order.
        return false;
    }

    const std::uint32_t messageId = nextMessageId_++;
    auto fragments =
        fragmentMessage(messageId, payload.data(), payload.size(), budget);
    if (fragments.empty()) {
        return false;
    }
    if (channelState.outbox.size() + fragments.size() > config_.outboxLimit) {
        return false;
    }
    for (auto& fragment : fragments) {
        channelState.outbox.push_back({std::move(fragment), kFlagFragment});
    }
    return true;
}

std::vector<Session::Delivery> Session::takeDelivered() {
    std::vector<Delivery> result = std::move(delivered_);
    delivered_.clear();
    return result;
}

std::size_t Session::inFlight(Channel channel) const {
    return state(channel).sender.pending();
}

std::size_t Session::outboxSize(Channel channel) const {
    return state(channel).outbox.size();
}

}  // namespace coopnet