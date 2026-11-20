#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
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

    // False when the peer is unknown or not joined yet.
    bool send(const Endpoint& to, Channel channel,
              std::vector<std::uint8_t> payload);
    // Returns how many joined peers accepted the message.
    std::size_t broadcast(Channel channel, const std::vector<std::uint8_t>& payload);

    bool isJoined(const Endpoint& endpoint) const;
    std::size_t peerCount() const { return peers_.size(); }
    std::size_t joinedCount() const;
    std::size_t droppedDatagrams() const { return droppedDatagrams_; }

private:
    bool resolveSlot(const Endpoint& self, std::uint8_t requested,
                     std::uint8_t& assigned) const;

    NetServerConfig config_;
    std::map<Endpoint, ServerPeer> peers_;
    std::size_t droppedDatagrams_ = 0;
};

}  // namespace coopnet