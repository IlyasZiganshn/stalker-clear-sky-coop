// Player transform replication over coopnet: each client publishes its own
// position at 10 Hz, the server stamps the author's net_id and rebroadcasts it
// to the other peers.
//
//   coopnet_world local [ms]
//   coopnet_world server [port]
//   coopnet_world client <port> <name> [ms]

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/player_state.h"
#include "coopnet/server.h"

namespace {

constexpr std::uint32_t kBuildHash = 0xB0B0B0B0u;
constexpr std::uint16_t kDefaultPort = 27027;
constexpr std::uint32_t kServerIdleTimeoutMs = 60000;
constexpr std::uint32_t kLocalRunMs = 3000;
constexpr std::uint32_t kSendIntervalMs = 100;
constexpr float kSpeed = 10.f; // m/s along +x for alpha, +z for bravo

std::uint32_t elapsedMs(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

coopnet::PeerConfig serverPeerConfig() {
    coopnet::PeerConfig config;
    config.buildHash = kBuildHash;
    config.maxPlayers = 4;
    return config;
}

coopnet::PeerConfig clientPeerConfig() {
    coopnet::PeerConfig config = serverPeerConfig();
    config.slot = 0xFF;
    return config;
}

coopnet::PlayerState sampleFor(const std::string& name, std::uint32_t netId,
                               std::uint32_t nowMs) {
    const float distance = kSpeed * static_cast<float>(nowMs) / 1000.f;
    coopnet::PlayerState state;
    state.netId = netId;
    state.y = 0.f;
    state.yaw = name == "alpha" ? 0.f : 1.57f;
    if (name == "alpha") {
        state.x = distance;
        state.z = 0.f;
    } else {
        state.x = 0.f;
        state.z = 100.f + distance;
    }
    return state;
}

int serveWorld(coopnet::UdpSocket& socket, std::atomic<bool>* stop = nullptr) {
    coopnet::NetServerConfig config;
    config.peer = serverPeerConfig();
    coopnet::NetServer server(config);

    const auto started = std::chrono::steady_clock::now();
    std::size_t relayed = 0;
    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (stop != nullptr && stop->load()) {
            std::printf("world server: relayed %zu states\n", relayed);
            return 0;
        }
        if (now > kServerIdleTimeoutMs) {
            std::printf("world server: overall timeout\n");
            return 1;
        }

        std::uint8_t buffer[2048];
        for (;;) {
            coopnet::Endpoint from;
            const std::int32_t received =
                socket.receiveFrom(buffer, sizeof(buffer), from);
            if (received < 0) {
                break;
            }
            server.onDatagram(from, buffer, static_cast<std::size_t>(received),
                              now);
        }

        for (const auto& delivery : server.takeDelivered()) {
            if (delivery.delivery.channel != coopnet::Channel::Control) {
                continue;
            }
            coopnet::PlayerState state;
            if (!coopnet::decodePlayerState(delivery.delivery.payload.data(),
                                            delivery.delivery.payload.size(),
                                            state)) {
                continue;
            }
            state.netId = server.netIdOf(delivery.from);
            if (state.netId == 0) {
                continue;
            }
            std::vector<std::uint8_t> wire;
            if (!coopnet::encodePlayerState(state, wire)) {
                continue;
            }
            if (server.broadcastExcept(delivery.from, coopnet::Channel::Control,
                                       wire) > 0) {
                ++relayed;
            }
        }

        for (const auto& outgoing : server.poll(now)) {
            socket.sendTo(outgoing.to, outgoing.data.data(),
                          outgoing.data.size());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct WorldClientStats {
    std::uint32_t netId = 0;
    std::size_t sent = 0;
    std::size_t received = 0;
    std::size_t malformed = 0;
    std::uint32_t remoteNetId = 0;
    coopnet::PlayerState lastRemote;
    bool haveRemote = false;
};

int runWorldClient(std::uint16_t port, const std::string& name,
                   std::uint32_t runMs, WorldClientStats& stats) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("world client %s: cannot open socket\n", name.c_str());
        return 1;
    }
    coopnet::ClientPeer peer(clientPeerConfig());
    if (!peer.start(name)) {
        std::printf("world client %s: cannot start handshake\n", name.c_str());
        return 1;
    }

    const coopnet::Endpoint server = coopnet::Endpoint::loopback(port);
    const auto started = std::chrono::steady_clock::now();
    std::uint32_t nextSendMs = 0;
    int result = 1;

    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (now > runMs) {
            break;
        }

        for (const auto& datagram : peer.poll(now)) {
            socket.sendTo(server, datagram.data(), datagram.size());
        }

        std::uint8_t buffer[2048];
        for (;;) {
            coopnet::Endpoint from;
            const std::int32_t received =
                socket.receiveFrom(buffer, sizeof(buffer), from);
            if (received < 0) {
                break;
            }
            peer.onDatagram(buffer, static_cast<std::size_t>(received), now);
        }

        if (peer.state() == coopnet::PeerState::Denied) {
            std::printf("world client %s: denied (reason %u)\n", name.c_str(),
                        static_cast<unsigned>(peer.denyReason()));
            break;
        }

        if (peer.state() == coopnet::PeerState::Joined) {
            stats.netId = peer.netId();
            if (now >= nextSendMs) {
                nextSendMs = now + kSendIntervalMs;
                const coopnet::PlayerState state =
                    sampleFor(name, stats.netId, now);
                std::vector<std::uint8_t> wire;
                if (coopnet::encodePlayerState(state, wire) &&
                    peer.send(coopnet::Channel::Control, wire)) {
                    ++stats.sent;
                }
            }
        }

        for (const auto& delivery : peer.takeDelivered()) {
            if (delivery.channel != coopnet::Channel::Control) {
                continue;
            }
            coopnet::PlayerState state;
            if (!coopnet::decodePlayerState(delivery.payload.data(),
                                            delivery.payload.size(), state)) {
                ++stats.malformed;
                continue;
            }
            ++stats.received;
            stats.remoteNetId = state.netId;
            stats.lastRemote = state;
            stats.haveRemote = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (peer.state() == coopnet::PeerState::Joined || stats.sent > 0) {
        result = 0;
    }
    if (stats.haveRemote) {
        std::printf(
            "world client %s: net %u, sent %zu, received %zu, malformed %zu, "
            "remote %u at %.1f %.1f %.1f, rtt %u ms\n",
            name.c_str(), stats.netId, stats.sent, stats.received,
            stats.malformed, stats.remoteNetId, stats.lastRemote.x,
            stats.lastRemote.y, stats.lastRemote.z, peer.session().rttMs());
    } else {
        std::printf(
            "world client %s: net %u, sent %zu, received 0, rtt %u ms\n",
            name.c_str(), stats.netId, stats.sent, peer.session().rttMs());
    }
    std::fflush(stdout);
    return result;
}

bool remoteLooksRight(const WorldClientStats& stats, std::uint32_t otherNetId,
                      const std::string& otherName) {
    if (!stats.haveRemote || stats.received < 4) {
        return false;
    }
    if (stats.remoteNetId != otherNetId) {
        return false;
    }
    const coopnet::PlayerState& last = stats.lastRemote;
    if (otherName == "bravo") {
        // bravo stands on x = 0 and walks along z from 100.
        return last.x == 0.f && last.z >= 100.f && last.z < 200.f;
    }
    // alpha walks along x and keeps z = 0.
    return last.z == 0.f && last.x >= 0.f && last.x < 100.f;
}

int runLocal(std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("world: cannot open server socket\n");
        return 1;
    }

    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&socket, &stop, &serverResult] {
        serverResult = serveWorld(socket, &stop);
    });

    WorldClientStats first;
    WorldClientStats second;
    int firstResult = 1;
    int secondResult = 1;
    std::thread firstThread([&socket, &first, &firstResult, runMs] {
        firstResult =
            runWorldClient(socket.localPort(), "alpha", runMs, first);
    });
    std::thread secondThread([&socket, &second, &secondResult, runMs] {
        secondResult =
            runWorldClient(socket.localPort(), "bravo", runMs, second);
    });

    firstThread.join();
    secondThread.join();
    stop.store(true);
    serverThread.join();

    const bool firstOk =
        firstResult == 0 && remoteLooksRight(first, second.netId, "bravo");
    const bool secondOk =
        secondResult == 0 && remoteLooksRight(second, first.netId, "alpha");

    if (firstOk && secondOk && serverResult == 0) {
        std::printf("coopnet_world: PASSED\n");
        return 0;
    }
    std::printf(
        "coopnet_world: FAILED (alpha %d, bravo %d, server %d, "
        "alpha got %zu, bravo got %zu)\n",
        firstResult, secondResult, serverResult, first.received,
        second.received);
    return 1;
}

bool parsePort(const char* text, std::uint16_t& port) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0 || value > 65535) {
        return false;
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

bool parseMs(const char* text, std::uint32_t& ms) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0) {
        return false;
    }
    ms = static_cast<std::uint32_t>(value);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("coopnet_world: network init failed\n");
        return 1;
    }

    if (argc < 2) {
        std::printf("usage:\n"
                    "  coopnet_world local [ms]\n"
                    "  coopnet_world server [port]\n"
                    "  coopnet_world client <port> <name> [ms]\n");
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "local") {
        std::uint32_t runMs = kLocalRunMs;
        if (argc > 2 && !parseMs(argv[2], runMs)) {
            std::printf("coopnet_world: bad duration\n");
            return 2;
        }
        return runLocal(runMs);
    }
    if (mode == "server") {
        std::uint16_t port = kDefaultPort;
        if (argc > 2 && !parsePort(argv[2], port)) {
            std::printf("coopnet_world: bad port\n");
            return 2;
        }
        coopnet::UdpSocket socket;
        if (!socket.open(port)) {
            std::printf("world server: cannot bind port %u\n", port);
            return 1;
        }
        return serveWorld(socket);
    }
    if (mode == "client") {
        std::uint16_t port = 0;
        if (argc < 4 || !parsePort(argv[2], port)) {
            std::printf("coopnet_world: bad port or name\n");
            return 2;
        }
        std::uint32_t runMs = kLocalRunMs;
        if (argc > 4 && !parseMs(argv[4], runMs)) {
            std::printf("coopnet_world: bad duration\n");
            return 2;
        }
        WorldClientStats stats;
        return runWorldClient(port, argv[3], runMs, stats);
    }

    std::printf("coopnet_world: unknown mode '%s'\n", mode.c_str());
    return 2;
}