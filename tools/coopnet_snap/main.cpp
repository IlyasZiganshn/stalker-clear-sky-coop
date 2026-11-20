// Live snapshot replication over real UDP: the server simulates a small world,
// every client acks the snapshot it holds and receives deltas against it.
//
//   coopnet_snap local [ms]           one process, server thread + client
//   coopnet_snap server [port]        stand-alone server
//   coopnet_snap client <port> [ms]   stand-alone client

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "coopnet/input.h"
#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/replicator.h"
#include "coopnet/server.h"
#include "coopnet/snapshot.h"
#include "coopnet/snapshot_store.h"

namespace {

constexpr std::uint32_t kBuildHash = 0xC0FFEEu;
constexpr std::uint16_t kDefaultPort = 27016;
constexpr std::uint32_t kSnapshotIntervalMs = 50;  // 20 Hz
constexpr std::uint32_t kInputIntervalMs = 100;    // 10 Hz: acks + input
constexpr std::uint32_t kMaxAckLagTicks = 20;      // 1 s at 20 Hz: full resync
constexpr std::uint32_t kDefaultRunMs = 3000;
constexpr std::uint32_t kServerIdleTimeoutMs = 60000;
constexpr std::uint32_t kEntityCount = 32;
constexpr float kWorldHalfSize = 90.0f;

std::uint32_t elapsedMs(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

coopnet::PeerConfig peerConfig(std::uint8_t slot = 0) {
    coopnet::PeerConfig config;
    config.buildHash = kBuildHash;
    config.sessionToken = 0x5EED0002u;
    config.maxPlayers = 4;
    config.slot = slot;
    return config;
}

coopnet::SnapshotCodec makeCodec() {
    return coopnet::SnapshotCodec(
        {coopnet::Vec3{-kWorldHalfSize, -10.0f, -kWorldHalfSize}},
        {coopnet::Vec3{kWorldHalfSize, 30.0f, kWorldHalfSize}});
}

coopnet::Snapshot makeWorld(std::uint32_t tick) {
    coopnet::Snapshot world;
    world.tick = tick;
    world.baseTick = 0;
    world.entities.reserve(kEntityCount);
    for (std::uint32_t i = 0; i < kEntityCount; ++i) {
        coopnet::EntityState entity;
        entity.netId = 1000 + i;
        entity.classId = 1;
        entity.present = coopnet::kFieldPosition | coopnet::kFieldAngles |
                         coopnet::kFieldHealth | coopnet::kFieldState;
        const float base = -60.0f + static_cast<float>(i % 8) * 15.0f;
        entity.position = {base + static_cast<float>(tick) * 0.05f, 1.0f,
                           -40.0f + static_cast<float>(i / 8) * 20.0f};
        entity.yaw = static_cast<float>(i) * 0.2f +
                     static_cast<float>(tick) * 0.01f;
        entity.health = static_cast<std::uint16_t>(100 - (tick + i) % 3);
        entity.state = static_cast<std::uint8_t>(tick % 4 == 0 ? 1 : 0);
        world.entities.push_back(entity);
    }
    return world;
}

bool encodeSnapshot(const coopnet::SnapshotCodec& codec,
                    const coopnet::Snapshot& snapshot,
                    std::vector<std::uint8_t>& out) {
    coopnet::BitWriter writer(1024);
    if (!codec.encode(snapshot, writer)) {
        return false;
    }
    out = writer.data();
    return true;
}

}  // namespace

int serve(coopnet::UdpSocket& socket, std::atomic<bool>* stop = nullptr,
          coopnet::InterestConfig interest = {}) {
    std::printf("snap server: listening on port %u\n", socket.localPort());
    std::fflush(stdout);

    coopnet::NetServerConfig config;
    config.peer = peerConfig();
    coopnet::NetServer server(config);
    const coopnet::SnapshotCodec codec = makeCodec();

    struct ClientState {
        explicit ClientState(coopnet::InterestConfig config)
            : replicator(config) {}

        coopnet::ClientReplicator replicator;
        std::uint32_t sentTick = 0;
        std::uint32_t ackedTick = 0;
        std::uint32_t actorNetId = 0;
    };
    std::map<coopnet::Endpoint, ClientState> clients;

    const auto started = std::chrono::steady_clock::now();
    std::uint32_t tick = 0;
    std::uint32_t nextSnapshotMs = 0;
    std::uint64_t snapshotsSent = 0;
    std::uint64_t deltasSent = 0;
    std::uint64_t sendFailures = 0;

    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (stop != nullptr && stop->load()) {
            return 0;
        }
        if (now > kServerIdleTimeoutMs) {
            std::printf("snap server: overall timeout\n");
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

        for (const auto& delivered : server.takeDelivered()) {
            if (delivered.delivery.channel != coopnet::Channel::Input) {
                continue;
            }
            coopnet::InputState input;
            if (!coopnet::decodeInput(delivered.delivery.payload.data(),
                                      delivered.delivery.payload.size(), input)) {
                continue;
            }
            auto it = clients.find(delivered.from);
            if (it == clients.end()) {
                // Actor identity comes from the handshake: net_id = 1000+slot.
                const std::uint32_t actorNetId =
                    server.netIdOf(delivered.from);
                if (actorNetId == 0 || actorNetId >= 1000 + kEntityCount) {
                    continue;
                }
                it = clients.try_emplace(delivered.from, interest).first;
                it->second.actorNetId = actorNetId;
            }
            it->second.ackedTick = input.ackSnapshot;
        }

        if (now >= nextSnapshotMs && now <= kServerIdleTimeoutMs) {
            nextSnapshotMs = now + kSnapshotIntervalMs;
            const coopnet::Snapshot world = makeWorld(++tick);
            for (auto& [endpoint, client] : clients) {
                // Deltas chain from the previous snapshot sent to this client.
                // If the client's acks fall too far behind (lost packet broke
                // the chain), or it never sent one, start over with a full one.
                const bool lagging =
                    client.ackedTick == 0 ||
                    tick - client.ackedTick > kMaxAckLagTicks;
                const std::uint32_t base = lagging ? 0 : client.sentTick;

                coopnet::Vec3 viewer{};
                for (const auto& entity : world.entities) {
                    if (entity.netId == client.actorNetId) {
                        viewer = entity.position;
                        break;
                    }
                }

                const coopnet::Snapshot message =
                    client.replicator.next(world, viewer, base);

                std::vector<std::uint8_t> datagram;
                if (!encodeSnapshot(codec, message, datagram)) {
                    continue;
                }
                if (!server.send(endpoint, coopnet::Channel::Snapshot, datagram)) {
                    ++sendFailures;
                    client.sentTick = 0;
                    continue;
                }
                client.sentTick = world.tick;
                ++snapshotsSent;
                if (message.baseTick != 0) {
                    ++deltasSent;
                }
            }
        }

        for (const auto& outgoing : server.poll(now)) {
            socket.sendTo(outgoing.to, outgoing.data.data(), outgoing.data.size());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int runClient(std::uint16_t port, std::uint32_t runMs,
              bool expectAllEntities = true, const char* tag = "snap client",
              std::uint8_t slot = 0) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("%s: cannot open socket\n", tag);
        return 1;
    }
    coopnet::ClientPeer peer(peerConfig(slot));
    if (!peer.start("snap-client")) {
        std::printf("%s: cannot start handshake\n", tag);
        return 1;
    }

    const coopnet::Endpoint server = coopnet::Endpoint::loopback(port);
    const coopnet::SnapshotCodec codec = makeCodec();
    coopnet::SnapshotStore store;

    const auto started = std::chrono::steady_clock::now();
    coopnet::InputState input;
    std::size_t applied = 0;
    std::size_t full = 0;
    std::size_t deltas = 0;
    std::size_t rejected = 0;
    std::uint32_t nextInputMs = 0;
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
            std::printf("%s: denied (reason %u)\n", tag,
                        static_cast<unsigned>(peer.denyReason()));
            break;
        }

        input.tick = now / kInputIntervalMs + 1;
        input.ackSnapshot = store.tick();
        if (peer.state() == coopnet::PeerState::Joined && now >= nextInputMs) {
            nextInputMs = now + kInputIntervalMs;
            peer.send(coopnet::Channel::Input, coopnet::encodeInput(input));
        }

        for (const auto& delivery : peer.takeDelivered()) {
            if (delivery.channel != coopnet::Channel::Snapshot) {
                continue;
            }
            coopnet::Snapshot snapshot;
            coopnet::BitReader reader(delivery.payload.data(),
                                      delivery.payload.size());
            if (!codec.decode(reader, snapshot)) {
                ++rejected;
                continue;
            }
            if (!store.apply(snapshot)) {
                ++rejected;
                continue;
            }
            ++applied;
            if (snapshot.baseTick == 0) {
                ++full;
            } else {
                ++deltas;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto& stats = peer.session().stats();
    std::printf(
        "%s: applied %zu snapshots (%zu full, %zu delta), entities "
        "%zu, rejected %zu, rtt %u ms, tx %llu B, rx %llu B\n",
        tag, applied, full, deltas, store.size(), rejected,
        peer.session().rttMs(),
        static_cast<unsigned long long>(stats.sentBytes),
        static_cast<unsigned long long>(stats.receivedBytes));
    std::fflush(stdout);

    const bool entitiesOk = expectAllEntities
        ? store.size() == kEntityCount
        : store.size() > 0 && store.size() < kEntityCount;
    if (applied > 0 && deltas > 0 && rejected <= 2 && entitiesOk) {
        result = 0;
    }
    return result;
}

int runLocal(std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("snap: cannot open server socket\n");
        return 1;
    }

    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&socket, &stop, &serverResult] {
        serverResult = serve(socket, &stop);
    });

    const int clientResult = runClient(socket.localPort(), runMs);
    stop.store(true);
    serverThread.join();

    if (clientResult == 0 && serverResult == 0) {
        std::printf("coopnet_snap: PASSED\n");
        return 0;
    }
    std::printf("coopnet_snap: FAILED (client %d, server %d)\n", clientResult,
                serverResult);
    return 1;
}

int runTwo(std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("snap: cannot open server socket\n");
        return 1;
    }

    // Clients drive actors 1000 and 1031; 70 m radius keeps their views apart.
    const coopnet::InterestConfig interest{70.0f, 85.0f};
    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&socket, &stop, &serverResult, interest] {
        serverResult = serve(socket, &stop, interest);
    });

    int firstResult = 1;
    int secondResult = 1;
    std::thread firstThread([&socket, &firstResult, runMs] {
        firstResult = runClient(socket.localPort(), runMs, false,
                                "snap client 1", 0xFF);
    });
    std::thread secondThread([&socket, &secondResult, runMs] {
        secondResult = runClient(socket.localPort(), runMs, false,
                                 "snap client 2", 0xFF);
    });

    firstThread.join();
    secondThread.join();
    stop.store(true);
    serverThread.join();

    if (firstResult == 0 && secondResult == 0 && serverResult == 0) {
        std::printf("coopnet_snap: PASSED (two clients)\n");
        return 0;
    }
    std::printf("coopnet_snap: FAILED (client 1 %d, client 2 %d, server %d)\n",
                firstResult, secondResult, serverResult);
    return 1;
}

int runServer(std::uint16_t port) {
    coopnet::UdpSocket socket;
    if (!socket.open(port)) {
        std::printf("snap server: cannot bind port %u\n", port);
        return 1;
    }
    return serve(socket);
}

namespace {

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

}  // namespace

int main(int argc, char** argv) {
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("coopnet_snap: network init failed\n");
        return 1;
    }

    if (argc < 2) {
        std::printf(
            "usage:\n"
            "  coopnet_snap local [ms]\n"
            "  coopnet_snap two [ms]\n"
            "  coopnet_snap server [port]\n"
            "  coopnet_snap client <port> [ms]\n");
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "local") {
        std::uint32_t runMs = kDefaultRunMs;
        if (argc > 2 && !parseMs(argv[2], runMs)) {
            std::printf("coopnet_snap: bad duration\n");
            return 2;
        }
        return runLocal(runMs);
    }
    if (mode == "two") {
        std::uint32_t runMs = kDefaultRunMs;
        if (argc > 2 && !parseMs(argv[2], runMs)) {
            std::printf("coopnet_snap: bad duration\n");
            return 2;
        }
        return runTwo(runMs);
    }
    if (mode == "server") {
        std::uint16_t port = kDefaultPort;
        if (argc > 2 && !parsePort(argv[2], port)) {
            std::printf("coopnet_snap: bad port\n");
            return 2;
        }
        return runServer(port);
    }
    if (mode == "client") {
        std::uint16_t port = 0;
        std::uint32_t runMs = kDefaultRunMs;
        if (argc < 3 || !parsePort(argv[2], port) ||
            (argc > 3 && !parseMs(argv[3], runMs))) {
            std::printf("coopnet_snap: client needs a port\n");
            return 2;
        }
        return runClient(port, runMs);
    }

    std::printf("coopnet_snap: unknown mode '%s'\n", mode.c_str());
    return 2;
}