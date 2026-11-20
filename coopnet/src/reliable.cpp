#include "coopnet/reliable.h"

#include <algorithm>

namespace coopnet {

bool ReliableSender::send(std::vector<std::uint8_t> payload, std::uint32_t nowMs,
                          std::uint16_t& outSequence, std::uint8_t flags) {
    if (queue_.size() >= kWindow) {
        return false;
    }
    if (!queue_.empty()) {
        const auto span =
            static_cast<std::uint16_t>(nextSequence_ - queue_.front().sequence);
        if (span >= kMaxSpan) {
            return false;
        }
    }

    Entry entry;
    entry.sequence = nextSequence_++;
    entry.flags = flags;
    entry.payload = std::move(payload);
    entry.nextSendMs = nowMs;
    entry.intervalMs = kInitialRtoMs;
    outSequence = entry.sequence;
    queue_.push_back(std::move(entry));
    return true;
}

void ReliableSender::onAck(std::uint16_t ack, std::uint32_t ackBits) {
    for (auto it = queue_.begin(); it != queue_.end();) {
        const std::uint16_t distance = static_cast<std::uint16_t>(ack - it->sequence);
        bool acked = false;
        if (distance == 0) {
            acked = true;
        } else if (distance <= 32) {
            acked = ((ackBits >> (distance - 1)) & 1u) != 0;
        }
        it = acked ? queue_.erase(it) : it + 1;
    }
}

std::vector<OutgoingMessage> ReliableSender::collect(std::uint32_t nowMs) {
    std::vector<OutgoingMessage> out;
    for (Entry& entry : queue_) {
        if (entry.nextSendMs > nowMs) {
            continue;
        }
        out.push_back({entry.sequence, entry.flags, entry.payload});
        entry.nextSendMs = nowMs + entry.intervalMs;
        entry.intervalMs = std::min(entry.intervalMs * 2, kMaxRtoMs);
    }
    return out;
}

std::vector<ReceivedMessage> ReliableReceiver::onPacket(
    std::uint16_t sequence, std::uint8_t flags, std::vector<std::uint8_t> payload) {
    std::vector<ReceivedMessage> delivered;

    const auto ahead = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(sequence - expected_));
    if (ahead < 0) {
        return delivered;  // duplicate or too old
    }
    if (static_cast<std::uint16_t>(ahead) >= kWindow) {
        return delivered;  // outside of the receive window
    }

    ack_.record(sequence);

    if (ahead == 0) {
        delivered.push_back({flags, std::move(payload)});
        ++expected_;
        for (;;) {
            auto it = pending_.find(expected_);
            if (it == pending_.end()) {
                break;
            }
            delivered.push_back({it->second.flags, std::move(it->second.payload)});
            pending_.erase(it);
            ++expected_;
        }
        return delivered;
    }

    // Duplicates are ignored: the first copy is authoritative.
    pending_.emplace(sequence, Pending{flags, std::move(payload)});
    return delivered;
}

}  // namespace coopnet