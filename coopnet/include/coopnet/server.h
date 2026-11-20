#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "coopnet/net.h"
#include "coopnet/peer.h"

namespace coopnet {

struct NetServerConfig {
    PeerConfig peer;          // template applied to every connected client
    std::size_t maxPeers = 8;  // handshakes and joined clients together
};

// One UDP socket, many clients: routes datagrams by source address, owns a
// ServerPeer per client and assigns player slots across them.
class NetServer {
public:
    explicit NetServer(NetServerConfig config = {});

    struct Outgoing {
        Endpoint to;
        std::vector<std::uint8_t> data;
    };

    struct PeerDelivery {
        Endpoint from;
        Session::Delivery delivery;
    };

    // Unknown addresses create a peer, up to maxPeers; the rest are dropped.
    void onDatagram(const Endpoint& from, const std::uint8_t* data,
                    std::size_t size, std::uint32_t nowMs);

    // Datagrams to transmit, per destination. Also expires silent peers.
    std::vector<Outgoing> poll(std::uint32_t nowMs);

    std::vector<PeerDelivery> takeDelivered();

    // Join/leave transitions since the previous call. A peer that timed out
    // appears as Left with the net_id it had while joined.
    struct ServerEvent {
        enum class Type { Joined, Left } type = Type::Joined;
        Endpoint endpoint;
        std::uint32_t netId = 0;
    };
    std::vector<ServerEvent> takeEvents();

    // False when the peer is unknown or not joined yet.
    bool send(const Endpoint& to, Channel channel,
              std::vector<std::uint8_t> payload);
    // Returns how many joined peers accepted the message.
    std::size_t broadcast(Channel channel, const std::vector<std::uint8_t>& payload);
    // Same, skipping one peer (a chat author does not need their own message).
    std::size_t broadcastExcept(const Endpoint& except, Channel channel,
                                const std::vector<std::uint8_t>& payload);

    bool isJoined(const Endpoint& endpoint) const;
    // Player identity assigned during the handshake; 0 when the peer is
    // unknown or has not joined yet.
    std::uint32_t netIdOf(const Endpoint& endpoint) const;
    // Name the peer sent in C_HELLO; empty when the peer is unknown or has
    // not joined yet.
    std::string nameOfNetId(std::uint32_t netId) const;
    std::size_t peerCount() const { return peers_.size(); }
    std::size_t joinedCount() const;
    std::size_t droppedDatagrams() const { return droppedDatagrams_; }

private:
    bool resolveSlot(const Endpoint& self, std::uint8_t requested,
                     std::uint8_t& assigned) const;

    NetServerConfig config_;
    std::map<Endpoint, ServerPeer> peers_;
    std::map<Endpoint, std::uint32_t> reportedJoined_;
    std::size_t droppedDatagrams_ = 0;
};

}  // namespace coopnet