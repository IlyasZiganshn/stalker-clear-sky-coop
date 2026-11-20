// Remote player spawn over coopnet: on join a client sends its serialized
// actor to the host, the host sends its own actor to the new peer and relays
// every client spawn to the others. Blobs are opaque here, as in the engine.
//
//   coopnet_spawn local [ms]
//   coopnet_spawn server [port]
//   coopnet_spawn client <port> <name> [ms]

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/server.h"
#include "coopnet/spawn.h"

namespace {

constexpr std::uint32_t kBuildHash = 0xB0B0B0B0u;
constexpr std::uint16_t kDefaultPort = 27037;
constexpr std::uint32_t kServerIdleTimeoutMs = 60000;
constexpr std::uint32_t kLocalRunMs = 3000;
constexpr std::uint32_t kHostNetId = 1;

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

std::vector<std::uint8_t> makeBlob(const std::string& name) {
    const std::string text = "CSE|" + name;
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

bool blobIs(const std::vector<std::uint8_t>& blob, const std::string& name) {
    const std::vector<std::uint8_t> expected = makeBlob(name);
    return blob == expected;
}

int serveSpawn(coopnet::UdpSocket& socket, std::atomic<bool>* stop = nullptr) {
    coopnet::NetServerConfig config;
    config.peer = serverPeerConfig();
    coopnet::NetServer server(config);

    coopnet::Spawn hostSpawn;
    hostSpawn.netId = kHostNetId;
    hostSpawn.data = makeBlob("host");

    // Known player spawns, so a late joiner receives everyone already in game.
    std::map<std::uint32_t, std::vector<std::uint8_t>> knownSpawns;

    const auto started = std::chrono::steady_clock::now();
    std::size_t relayed = 0;
    std::size_t hostSent = 0;
    std::size_t malformed = 0;
    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (stop != nullptr && stop->load()) {
            std::printf("spawn server: relayed %zu spawns, host spawns %zu, malformed %zu\n",
                        relayed, hostSent, malformed);
            return 0;
        }
        if (now > kServerIdleTimeoutMs) {
            std::printf("spawn server: overall timeout\n");
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
            coopnet::Spawn spawn;
            if (!coopnet::decodeSpawn(delivery.delivery.payload.data(),
                                      delivery.delivery.payload.size(), spawn)) {
                ++malformed;
                continue;
            }
            spawn.netId = server.netIdOf(delivery.from);
            if (spawn.netId == 0) {
                continue;
            }
            std::vector<std::uint8_t> wire;
            if (!coopnet::encodeSpawn(spawn, wire)) {
                ++malformed;
                continue;
            }
            knownSpawns[spawn.netId] = spawn.data;
            if (server.broadcastExcept(delivery.from, coopnet::Channel::Control,
                                       wire) > 0) {
                ++relayed;
            }
        }

        for (const auto& event : server.takeEvents()) {
            if (event.type == coopnet::NetServer::ServerEvent::Type::Left) {
                knownSpawns.erase(event.netId);
                continue;
            }
            std::vector<std::uint8_t> wire;
            if (coopnet::encodeSpawn(hostSpawn, wire) &&
                server.send(event.endpoint, coopnet::Channel::Control, wire)) {
                ++hostSent;
            }
            for (const auto& known : knownSpawns) {
                coopnet::Spawn spawn;
                spawn.netId = known.first;
                spawn.data = known.second;
                if (coopnet::encodeSpawn(spawn, wire)) {
                    server.send(event.endpoint, coopnet::Channel::Control, wire);
                }
            }
        }

        for (const auto& outgoing : server.poll(now)) {
            socket.sendTo(outgoing.to, outgoing.data.data(),
                          outgoing.data.size());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct SpawnClientStats {
    std::uint32_t netId = 0;
    std::size_t spawnsSent = 0;
    std::map<std::uint32_t, std::vector<std::uint8_t>> received;
    std::size_t malformed = 0;
};

int runSpawnClient(std::uint16_t port, const std::string& name,
                   std::uint32_t runMs, SpawnClientStats& stats) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("spawn client %s: cannot open socket\n", name.c_str());
        return 1;
    }
    coopnet::ClientPeer peer(clientPeerConfig());
    if (!peer.start(name)) {
        std::printf("spawn client %s: cannot start handshake\n", name.c_str());
        return 1;
    }

    const coopnet::Endpoint server = coopnet::Endpoint::loopback(port);
    const auto started = std::chrono::steady_clock::now();
    bool spawnSent = false;
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
            std::printf("spawn client %s: denied (reason %u)\n", name.c_str(),
                        static_cast<unsigned>(peer.denyReason()));
            break;
        }

        if (peer.state() == coopnet::PeerState::Joined) {
            stats.netId = peer.netId();
            if (!spawnSent) {
                coopnet::Spawn own;
                own.netId = stats.netId;
                own.data = makeBlob(name);
                std::vector<std::uint8_t> wire;
                if (coopnet::encodeSpawn(own, wire) &&
                    peer.send(coopnet::Channel::Control, wire)) {
                    spawnSent = true;
                    ++stats.spawnsSent;
                }
            }
        }

        for (const auto& delivery : peer.takeDelivered()) {
            if (delivery.channel != coopnet::Channel::Control) {
                continue;
            }
            coopnet::Spawn spawn;
            if (!coopnet::decodeSpawn(delivery.payload.data(),
                                      delivery.payload.size(), spawn)) {
                ++stats.malformed;
                continue;
            }
            if (spawn.netId == stats.netId) {
                continue;
            }
            stats.received[spawn.netId] = std::move(spawn.data);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (peer.state() == coopnet::PeerState::Joined || stats.spawnsSent > 0) {
        result = 0;
    }

    std::printf("spawn client %s: net %u, sent %zu, received %zu, malformed %zu",
                name.c_str(), stats.netId, stats.spawnsSent,
                stats.received.size(), stats.malformed);
    for (const auto& entry : stats.received) {
        std::printf(", net %u blob %zu bytes", entry.first, entry.second.size());
    }
    std::printf(", rtt %u ms\n", peer.session().rttMs());
    std::fflush(stdout);
    return result;
}

int runLocal(std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("spawn: cannot open server socket\n");
        return 1;
    }

    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&socket, &stop, &serverResult] {
        serverResult = serveSpawn(socket, &stop);
    });

    SpawnClientStats first;
    SpawnClientStats second;
    int firstResult = 1;
    int secondResult = 1;
    std::thread firstThread([&socket, &first, &firstResult, runMs] {
        firstResult = runSpawnClient(socket.localPort(), "alpha", runMs, first);
    });
    std::thread secondThread([&socket, &second, &secondResult, runMs] {
        secondResult = runSpawnClient(socket.localPort(), "bravo", runMs, second);
    });

    firstThread.join();
    secondThread.join();
    stop.store(true);
    serverThread.join();

    const auto hostSeen = [](const SpawnClientStats& stats) {
        const auto it = stats.received.find(kHostNetId);
        return it != stats.received.end() && blobIs(it->second, "host");
    };
    const auto peerSeen = [](const SpawnClientStats& stats,
                             const std::string& peerName) {
        for (const auto& entry : stats.received) {
            if (entry.first != kHostNetId && blobIs(entry.second, peerName)) {
                return true;
            }
        }
        return false;
    };

    const bool firstOk = firstResult == 0 && first.spawnsSent == 1 &&
        first.malformed == 0 && hostSeen(first) && peerSeen(first, "bravo");
    const bool secondOk = secondResult == 0 && second.spawnsSent == 1 &&
        second.malformed == 0 && hostSeen(second) && peerSeen(second, "alpha");

    if (firstOk && secondOk && serverResult == 0) {
        std::printf("coopnet_spawn: PASSED\n");
        return 0;
    }
    std::printf(
        "coopnet_spawn: FAILED (alpha %d, bravo %d, server %d, "
        "alpha got host %d peer %d, bravo got host %d peer %d)\n",
        firstResult, secondResult, serverResult, int(hostSeen(first)),
        int(peerSeen(first, "bravo")), int(hostSeen(second)),
        int(peerSeen(second, "alpha")));
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
        std::printf("coopnet_spawn: network init failed\n");
        return 1;
    }

    if (argc < 2) {
        std::printf("usage:\n"
                    "  coopnet_spawn local [ms]\n"
                    "  coopnet_spawn server [port]\n"
                    "  coopnet_spawn client <port> <name> [ms]\n");
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "local") {
        std::uint32_t runMs = kLocalRunMs;
        if (argc > 2 && !parseMs(argv[2], runMs)) {
            std::printf("coopnet_spawn: bad duration\n");
            return 2;
        }
        return runLocal(runMs);
    }
    if (mode == "server") {
        std::uint16_t port = kDefaultPort;
        if (argc > 2 && !parsePort(argv[2], port)) {
            std::printf("coopnet_spawn: bad port\n");
            return 2;
        }
        coopnet::UdpSocket socket;
        if (!socket.open(port)) {
            std::printf("spawn server: cannot bind port %u\n", port);
            return 1;
        }
        return serveSpawn(socket);
    }
    if (mode == "client") {
        std::uint16_t port = 0;
        if (argc < 4 || !parsePort(argv[2], port)) {
            std::printf("coopnet_spawn: bad port or name\n");
            return 2;
        }
        std::uint32_t runMs = kLocalRunMs;
        if (argc > 4 && !parseMs(argv[4], runMs)) {
            std::printf("coopnet_spawn: bad duration\n");
            return 2;
        }
        SpawnClientStats stats;
        return runSpawnClient(port, argv[3], runMs, stats);
    }

    std::printf("coopnet_spawn: unknown mode '%s'\n", mode.c_str());
    return 2;
}