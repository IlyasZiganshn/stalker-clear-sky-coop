#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "coopnet/handshake.h"
#include "coopnet/session.h"

namespace coopnet {

enum class PeerState : std::uint8_t {
    Idle,
    HelloSent,      // client: C_HELLO queued, waiting for S_HELLO_ACK
    Acked,          // client: C_JOIN queued, waiting for S_JOIN_OK
    HelloReceived,  // server: S_HELLO_ACK queued, waiting for C_JOIN
    Joined,
    Denied,
};

struct PeerConfig {
    SessionConfig session;
    std::uint32_t buildHash = 0;
    std::uint32_t sessionToken = 0;  // server: value sent in S_HELLO_ACK
    std::uint8_t slot = 0;           // client: requested slot
    std::uint8_t maxPlayers = 4;     // server: slots it accepts
    std::uint16_t snapshotHz = 20;   // server: announced in S_HELLO_ACK
    std::uint16_t tickHz = 60;       // server: announced in S_HELLO_ACK
    ServerRules rules;               // server: sent in S_JOIN_OK
};

// Client side of the handshake plus application messaging. Handshake messages
// are consumed; everything else is handed to the caller in order.
class ClientPeer {
public:
    explicit ClientPeer(PeerConfig config = {});

    // Queues C_HELLO. False when the peer was already started or the name is
    // not representable on the wire.
    bool start(const std::string& name, std::uint32_t token = 0);

    void onDatagram(const std::uint8_t* data, std::size_t size,
                    std::uint32_t nowMs);
    std::vector<std::vector<std::uint8_t>> poll(std::uint32_t nowMs);

    // Application message; only accepted once joined.
    bool send(Channel channel, std::vector<std::uint8_t> payload);
    std::vector<Session::Delivery> takeDelivered();

    PeerState state() const { return state_; }
    JoinDenyReason denyReason() const { return denyReason_; }
    const ServerRules& rules() const { return rules_; }
    std::uint32_t netId() const { return netId_; }
    std::uint32_t sessionToken() const { return sessionToken_; }
    std::uint32_t protocolViolations() const { return protocolViolations_; }
    bool timedOut(std::uint32_t nowMs) const { return session_.timedOut(nowMs); }

    Session& session() { return session_; }
    const Session& session() const { return session_; }

private:
    void handleControl(Session::Delivery delivery);

    PeerConfig config_;
    Session session_;
    PeerState state_ = PeerState::Idle;
    JoinDenyReason denyReason_ = JoinDenyReason::ServerFull;
    ServerRules rules_;
    std::uint32_t netId_ = 0;
    std::uint32_t sessionToken_ = 0;
    std::uint32_t protocolViolations_ = 0;
    std::vector<Session::Delivery> pending_;
};

// Server side for a single peer. Slot bookkeeping across peers belongs to the
// caller: one ServerPeer handles one connected client.
class ServerPeer {
public:
    explicit ServerPeer(PeerConfig config = {});

    void onDatagram(const std::uint8_t* data, std::size_t size,
                    std::uint32_t nowMs);
    std::vector<std::vector<std::uint8_t>> poll(std::uint32_t nowMs);

    bool send(Channel channel, std::vector<std::uint8_t> payload);
    std::vector<Session::Delivery> takeDelivered();

    PeerState state() const { return state_; }
    const std::string& playerName() const { return playerName_; }
    std::uint8_t slot() const { return slot_; }
    std::uint32_t clientId() const { return clientId_; }
    std::uint32_t netId() const { return netId_; }
    std::uint32_t protocolViolations() const { return protocolViolations_; }
    bool timedOut(std::uint32_t nowMs) const { return session_.timedOut(nowMs); }

    Session& session() { return session_; }
    const Session& session() const { return session_; }

private:
    void handleControl(Session::Delivery delivery);
    void deny(JoinDenyReason reason);

    PeerConfig config_;
    Session session_;
    PeerState state_ = PeerState::Idle;
    std::string playerName_;
    std::uint8_t slot_ = 0;
    std::uint32_t clientId_ = 0;
    std::uint32_t netId_ = 0;
    std::uint32_t sessionToken_ = 0;
    std::uint32_t protocolViolations_ = 0;
    std::vector<Session::Delivery> pending_;
};

}  // namespace coopnet