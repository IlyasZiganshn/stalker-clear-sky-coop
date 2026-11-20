#include "coopnet/server.h"

#include <utility>

namespace coopnet {

NetServer::NetServer(NetServerConfig config) : config_(std::move(config)) {}

void NetServer::onDatagram(const Endpoint& from, const std::uint8_t* data,
                           std::size_t size, std::uint32_t nowMs) {
    auto it = peers_.find(from);
    if (it == peers_.end()) {
        if (peers_.size() >= config_.maxPeers) {
            ++droppedDatagrams_;
            return;
        }
        it = peers_
                 .emplace(from,
                          ServerPeer(config_.peer,
                                     [this, from](std::uint8_t requested,
                                                  std::uint8_t& assigned) {
                                         return resolveSlot(from, requested,
                                                            assigned);
                                     }))
                 .first;
    }
    it->second.onDatagram(data, size, nowMs);
}

std::vector<NetServer::Outgoing> NetServer::poll(std::uint32_t nowMs) {
    std::vector<Outgoing> out;
    for (auto it = peers_.begin(); it != peers_.end();) {
        for (const auto& datagram : it->second.poll(nowMs)) {
            out.push_back({it->first, datagram});
        }
        if (it->second.timedOut(nowMs)) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
    return out;
}

std::vector<NetServer::PeerDelivery> NetServer::takeDelivered() {
    std::vector<PeerDelivery> out;
    for (auto& [endpoint, peer] : peers_) {
        for (auto& delivery : peer.takeDelivered()) {
            out.push_back({endpoint, std::move(delivery)});
        }
    }
    return out;
}

bool NetServer::send(const Endpoint& to, Channel channel,
                     std::vector<std::uint8_t> payload) {
    auto it = peers_.find(to);
    if (it == peers_.end()) {
        return false;
    }
    return it->second.send(channel, std::move(payload));
}

std::size_t NetServer::broadcast(Channel channel,
                                 const std::vector<std::uint8_t>& payload) {
    std::size_t sent = 0;
    for (auto& [endpoint, peer] : peers_) {
        if (peer.state() == PeerState::Joined && peer.send(channel, payload)) {
            ++sent;
        }
    }
    return sent;
}

std::size_t NetServer::broadcastExcept(const Endpoint& except, Channel channel,
                                       const std::vector<std::uint8_t>& payload) {
    std::size_t sent = 0;
    for (auto& [endpoint, peer] : peers_) {
        if (endpoint == except) {
            continue;
        }
        if (peer.state() == PeerState::Joined && peer.send(channel, payload)) {
            ++sent;
        }
    }
    return sent;
}

bool NetServer::isJoined(const Endpoint& endpoint) const {
    const auto it = peers_.find(endpoint);
    return it != peers_.end() && it->second.state() == PeerState::Joined;
}

std::uint32_t NetServer::netIdOf(const Endpoint& endpoint) const {
    const auto it = peers_.find(endpoint);
    if (it == peers_.end() || it->second.state() != PeerState::Joined) {
        return 0;
    }
    return it->second.netId();
}

std::size_t NetServer::joinedCount() const {
    std::size_t count = 0;
    for (const auto& [endpoint, peer] : peers_) {
        if (peer.state() == PeerState::Joined) {
            ++count;
        }
    }
    return count;
}

bool NetServer::resolveSlot(const Endpoint& self, std::uint8_t requested,
                            std::uint8_t& assigned) const {
    for (std::uint8_t slot = 0; slot < config_.peer.maxPlayers; ++slot) {
        if (requested != 0xFF && requested != slot) {
            continue;
        }
        bool taken = false;
        for (const auto& [endpoint, peer] : peers_) {
            if (endpoint == self) {
                continue;
            }
            if (peer.state() == PeerState::Joined && peer.slot() == slot) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            assigned = slot;
            return true;
        }
    }
    return false;
}

}  // namespace coopnet