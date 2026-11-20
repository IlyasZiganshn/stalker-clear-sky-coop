#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coopnet {

// Handshake runs on the control channel before any gameplay traffic.
enum class HandshakeOpcode : std::uint8_t {
    ClientHello = 1,
    ServerHelloAck = 2,
    ClientJoin = 3,
    ServerJoinOk = 4,
    ServerJoinDeny = 5,
};

enum class JoinDenyReason : std::uint8_t {
    VersionMismatch = 1,
    BuildMismatch = 2,
    ServerFull = 3,
    BadName = 4,
    Busy = 5,
    Banned = 6,
};

// Longest player name accepted on the wire.
constexpr std::size_t kMaxNameBytes = 32;

struct ServerRules {
    std::uint8_t difficulty = 0;
    std::uint8_t maxPlayers = 2;
    bool friendlyFire = false;
    std::uint8_t timeSpeedPercent = 100;
};

struct ClientHello {
    std::uint16_t protocol = 0;
    std::uint32_t buildHash = 0;
    std::string name;
    std::uint32_t token = 0;  // 0 on first connect, server token on reconnect
};

struct ServerHelloAck {
    std::uint32_t clientId = 0;
    std::uint32_t session = 0;
    std::uint16_t snapshotHz = 20;
    std::uint16_t tickHz = 60;
};

struct ClientJoin {
    std::uint8_t slot = 0;
};

struct ServerJoinOk {
    std::uint32_t netId = 0;
    ServerRules rules;
};

struct ServerJoinDeny {
    JoinDenyReason reason = JoinDenyReason::ServerFull;
};

// Each encoder prefixes the message with its opcode. Returns an empty vector
// when the message cannot be represented (e.g. a name over kMaxNameBytes).
std::vector<std::uint8_t> encodeClientHello(const ClientHello& message);
std::vector<std::uint8_t> encodeServerHelloAck(const ServerHelloAck& message);
std::vector<std::uint8_t> encodeClientJoin(const ClientJoin& message);
std::vector<std::uint8_t> encodeServerJoinOk(const ServerJoinOk& message);
std::vector<std::uint8_t> encodeServerJoinDeny(const ServerJoinDeny& message);

// Strict: the opcode must match, all fields must be present and no trailing
// bytes are allowed.
bool decodeClientHello(const std::uint8_t* data, std::size_t size,
                       ClientHello& out);
bool decodeServerHelloAck(const std::uint8_t* data, std::size_t size,
                          ServerHelloAck& out);
bool decodeClientJoin(const std::uint8_t* data, std::size_t size,
                      ClientJoin& out);
bool decodeServerJoinOk(const std::uint8_t* data, std::size_t size,
                        ServerJoinOk& out);
bool decodeServerJoinDeny(const std::uint8_t* data, std::size_t size,
                          ServerJoinDeny& out);

}  // namespace coopnet