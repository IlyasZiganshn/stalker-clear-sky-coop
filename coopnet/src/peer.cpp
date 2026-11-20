#include "coopnet/peer.h"

#include <utility>

namespace coopnet {

namespace {

bool isHandshakeMessage(const std::vector<std::uint8_t>& payload) {
    if (payload.empty()) {
        return false;
    }
    const std::uint8_t opcode = payload[0];
    return opcode >= static_cast<std::uint8_t>(HandshakeOpcode::ClientHello) &&
           opcode <= static_cast<std::uint8_t>(HandshakeOpcode::ServerJoinDeny);
}

}  // namespace

ClientPeer::ClientPeer(PeerConfig config)
    : config_(std::move(config)), session_(config_.session) {}

bool ClientPeer::start(const std::string& name, std::uint32_t token) {
    if (state_ != PeerState::Idle) {
        return false;
    }
    ClientHello hello;
    hello.protocol = kProtocolVersion;
    hello.buildHash = config_.buildHash;
    hello.name = name;
    hello.token = token;

    auto payload = encodeClientHello(hello);
    if (payload.empty()) {
        return false;
    }
    if (!session_.send(Channel::Control, std::move(payload))) {
        return false;
    }
    state_ = PeerState::HelloSent;
    return true;
}

void ClientPeer::onDatagram(const std::uint8_t* data, std::size_t size,
                            std::uint32_t nowMs) {
    session_.onDatagram(data, size, nowMs);
    for (auto& delivery : session_.takeDelivered()) {
        if (delivery.channel == Channel::Control &&
            isHandshakeMessage(delivery.payload)) {
            handleControl(std::move(delivery));
        } else {
            pending_.push_back(std::move(delivery));
        }
    }
}

void ClientPeer::handleControl(Session::Delivery delivery) {
    switch (static_cast<HandshakeOpcode>(delivery.payload[0])) {
        case HandshakeOpcode::ServerHelloAck: {
            ServerHelloAck ack;
            if (!decodeServerHelloAck(delivery.payload.data(),
                                      delivery.payload.size(), ack)) {
                ++protocolViolations_;
                return;
            }
            sessionToken_ = ack.session;
            if (state_ != PeerState::HelloSent) {
                return;  // duplicate ack: reliable delivery makes it rare
            }
            ClientJoin join;
            join.slot = config_.slot;
            auto payload = encodeClientJoin(join);
            if (!payload.empty() &&
                session_.send(Channel::Control, std::move(payload))) {
                state_ = PeerState::Acked;
            }
            return;
        }
        case HandshakeOpcode::ServerJoinOk: {
            ServerJoinOk ok;
            if (!decodeServerJoinOk(delivery.payload.data(),
                                    delivery.payload.size(), ok)) {
                ++protocolViolations_;
                return;
            }
            netId_ = ok.netId;
            rules_ = ok.rules;
            state_ = PeerState::Joined;
            return;
        }
        case HandshakeOpcode::ServerJoinDeny: {
            ServerJoinDeny deny;
            if (!decodeServerJoinDeny(delivery.payload.data(),
                                      delivery.payload.size(), deny)) {
                ++protocolViolations_;
                return;
            }
            denyReason_ = deny.reason;
            state_ = PeerState::Denied;
            return;
        }
        default:
            // Client-only opcodes on the client side are a protocol violation.
            ++protocolViolations_;
            return;
    }
}

std::vector<std::vector<std::uint8_t>> ClientPeer::poll(std::uint32_t nowMs) {
    return session_.poll(nowMs);
}

bool ClientPeer::send(Channel channel, std::vector<std::uint8_t> payload) {
    if (state_ != PeerState::Joined) {
        return false;
    }
    return session_.send(channel, std::move(payload));
}

std::vector<Session::Delivery> ClientPeer::takeDelivered() {
    std::vector<Session::Delivery> result = std::move(pending_);
    pending_.clear();
    return result;
}

ServerPeer::ServerPeer(PeerConfig config)
    : config_(std::move(config)), session_(config_.session) {}

void ServerPeer::onDatagram(const std::uint8_t* data, std::size_t size,
                            std::uint32_t nowMs) {
    session_.onDatagram(data, size, nowMs);
    for (auto& delivery : session_.takeDelivered()) {
        if (delivery.channel == Channel::Control &&
            isHandshakeMessage(delivery.payload)) {
            handleControl(std::move(delivery));
        } else {
            pending_.push_back(std::move(delivery));
        }
    }
}

void ServerPeer::handleControl(Session::Delivery delivery) {
    switch (static_cast<HandshakeOpcode>(delivery.payload[0])) {
        case HandshakeOpcode::ClientHello: {
            if (state_ != PeerState::Idle) {
                return;  // hello retransmitted at the session level duplicates
            }
            ClientHello hello;
            if (!decodeClientHello(delivery.payload.data(),
                                   delivery.payload.size(), hello)) {
                ++protocolViolations_;
                return;
            }
            if (hello.protocol != kProtocolVersion) {
                deny(JoinDenyReason::VersionMismatch);
                return;
            }
            if (hello.buildHash != config_.buildHash) {
                deny(JoinDenyReason::BuildMismatch);
                return;
            }

            playerName_ = hello.name;
            clientId_ = 1;
            ServerHelloAck ack;
            ack.clientId = clientId_;
            ack.session = config_.sessionToken;
            ack.snapshotHz = config_.snapshotHz;
            ack.tickHz = config_.tickHz;
            const auto payload = encodeServerHelloAck(ack);
            if (payload.empty() ||
                !session_.send(Channel::Control, payload)) {
                return;
            }
            state_ = PeerState::HelloReceived;
            return;
        }
        case HandshakeOpcode::ClientJoin: {
            if (state_ != PeerState::HelloReceived) {
                return;
            }
            ClientJoin join;
            if (!decodeClientJoin(delivery.payload.data(), delivery.payload.size(),
                                  join)) {
                ++protocolViolations_;
                return;
            }
            if (join.slot >= config_.maxPlayers) {
                deny(JoinDenyReason::ServerFull);
                return;
            }

            slot_ = join.slot;
            ServerJoinOk ok;
            ok.netId = static_cast<std::uint32_t>(1000u + slot_);
            ok.rules = config_.rules;
            const auto payload = encodeServerJoinOk(ok);
            if (payload.empty() ||
                !session_.send(Channel::Control, payload)) {
                return;
            }
            state_ = PeerState::Joined;
            return;
        }
        default:
            // Server-only opcodes on the server side are a protocol violation.
            ++protocolViolations_;
            return;
    }
}

void ServerPeer::deny(JoinDenyReason reason) {
    ServerJoinDeny deny;
    deny.reason = reason;
    const auto payload = encodeServerJoinDeny(deny);
    if (!payload.empty()) {
        session_.send(Channel::Control, payload);
    }
    state_ = PeerState::Denied;
}

std::vector<std::vector<std::uint8_t>> ServerPeer::poll(std::uint32_t nowMs) {
    return session_.poll(nowMs);
}

bool ServerPeer::send(Channel channel, std::vector<std::uint8_t> payload) {
    if (state_ != PeerState::Joined) {
        return false;
    }
    return session_.send(channel, std::move(payload));
}

std::vector<Session::Delivery> ServerPeer::takeDelivered() {
    std::vector<Session::Delivery> result = std::move(pending_);
    pending_.clear();
    return result;
}

}  // namespace coopnet