// Movement prediction over real UDP: the server applies C_INPUT to player
// actors, clients predict their own actor and reconcile against snapshots.
//
//   coopnet_move local [ticks]   deterministic sim: latency, loss, corrections
//   coopnet_move two [ms]        two clients over loopback UDP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <string>
#include <thread>

#include "coopnet/actor.h"
#include "coopnet/client_predictor.h"
#include "coopnet/input.h"
#include "coopnet/interp.h"
#include "coopnet/movement.h"
#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/replicator.h"
#include "coopnet/server.h"
#include "coopnet/snapshot.h"
#include "coopnet/snapshot_store.h"

namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr std::uint32_t kLatencyTicks = 4;   // ~67 ms each way, local sim only
constexpr std::uint32_t kSnapshotEvery = 3;  // 20 Hz against a 60 Hz tick
constexpr std::uint32_t kKnockbackTick = 90;
constexpr std::uint32_t kDefaultTicks = 600;

constexpr std::uint32_t kBuildHash = 0xC0FFEEu;
constexpr std::uint16_t kDefaultPort = 27017;
constexpr std::uint32_t kSnapshotIntervalMs = 50;  // 20 Hz
constexpr std::uint32_t kInputIntervalMs = 16;     // ~60 Hz
constexpr std::uint32_t kMaxAckLagTicks = 20;
constexpr std::uint32_t kDefaultRunMs = 3000;
constexpr std::uint32_t kServerIdleTimeoutMs = 60000;
constexpr float kWorldHalfSize = 90.0f;

struct SnapshotMessage {
    coopnet::MoveState state;
    std::uint32_t ackInput = 0;
};

coopnet::InputState makeInput(std::uint32_t tick) {
    coopnet::InputState input;
    input.tick = tick;
    const std::uint32_t phase = tick % 240;
    if (phase < 120) {
        input.moveY = 1.0f;
        input.viewYaw = 0.0f;
    } else {
        input.moveY = 0.6f;
        input.moveX = 0.4f;
        input.viewYaw = 0.8f;
    }
    if (phase >= 60 && phase < 120) {
        input.buttons |= coopnet::kButtonSprint;
    }
    if (tick % 50 == 0) {
        input.buttons |= coopnet::kButtonJump;
    }
    return input;
}

// A closed path near the origin: 1 s forward, 1 s back, heading turns every
// 2 s. Keeps the actor inside the level bounds and the interest radius.
coopnet::InputState makeMoveInput(std::uint32_t tick, std::uint32_t ack) {
    coopnet::InputState input;
    input.tick = tick;
    input.ackSnapshot = ack;
    const std::uint32_t phase = tick % 120;
    input.moveY = phase < 60 ? 1.0f : -1.0f;
    input.viewYaw = (tick / 120) % 2 == 0 ? 0.0f : 1.3f;
    if (phase >= 30 && phase < 60) {
        input.buttons |= coopnet::kButtonSprint;
    }
    if (tick % 53 == 0) {
        input.buttons |= coopnet::kButtonJump;
    }
    return input;
}

std::uint32_t elapsedMs(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

coopnet::PeerConfig peerConfig(std::uint8_t slot = 0) {
    coopnet::PeerConfig config;
    config.buildHash = kBuildHash;
    config.sessionToken = 0x5EED0003u;
    config.maxPlayers = 4;
    config.slot = slot;
    return config;
}

coopnet::SnapshotCodec makeCodec() {
    return coopnet::SnapshotCodec(
        {coopnet::Vec3{-kWorldHalfSize, -10.0f, -kWorldHalfSize}},
        {coopnet::Vec3{kWorldHalfSize, 30.0f, kWorldHalfSize}});
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

bool parseTicks(const char* text, std::uint32_t& out) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0 || value > 100000) {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool parseMs(const char* text, std::uint32_t& out) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0) {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

}  // namespace

int runLocal(std::uint32_t ticks) {
    const coopnet::MovementConfig config;
    coopnet::ClientPredictor client(config, kDt);

    coopnet::MoveState serverState;
    std::uint32_t lastAppliedInput = 0;
    std::map<std::uint32_t, coopnet::MoveState> serverAfterInput;
    std::map<std::uint32_t, coopnet::InputState> uplink;
    std::map<std::uint32_t, SnapshotMessage> downlink;

    std::uint32_t applied = 0;
    std::uint32_t dropped = 0;
    std::uint32_t corrections = 0;
    float maxCorrection = 0.0f;

    const std::uint32_t lastTick = ticks + kLatencyTicks + kSnapshotEvery + 2;
    for (std::uint32_t tick = 1; tick <= lastTick; ++tick) {
        if (auto it = downlink.find(tick); it != downlink.end()) {
            ++applied;
            const float error =
                client.reconcile(it->second.state, it->second.ackInput);
            if (error > 1e-4f) {
                ++corrections;
                maxCorrection = std::max(maxCorrection, error);
            }
            downlink.erase(it);
        }

        if (tick <= ticks) {
            const coopnet::InputState input = makeInput(tick);
            client.predict(input);
            uplink[tick + kLatencyTicks] = input;
        }

        if (auto it = uplink.find(tick); it != uplink.end()) {
            const coopnet::InputState& input = it->second;
            if (input.tick == kKnockbackTick) {
                // Something the client cannot predict: an explosion pushes
                // the actor. The next snapshot must correct it.
                serverState.velocity.x += 3.0f;
                serverState.velocity.y += 2.0f;
                serverState.onGround = false;
            }
            coopnet::stepMovement(serverState, input, config, kDt);
            lastAppliedInput = input.tick;
            serverAfterInput[input.tick] = serverState;
            uplink.erase(it);
        }

        const bool snapshotDue = tick % kSnapshotEvery == 0;
        if (snapshotDue && tick <= ticks) {
            const bool lose = tick % 21 == 0 && tick != ticks;
            if (lose) {
                ++dropped;
            } else {
                downlink[tick + kLatencyTicks] = SnapshotMessage{
                    serverState, lastAppliedInput};
            }
        }
    }

    const auto expected = serverAfterInput.find(ticks);
    if (expected == serverAfterInput.end()) {
        std::printf("move: server never applied input %u\n", ticks);
        return 1;
    }
    const coopnet::Vec3& finalClient = client.state().position;
    const coopnet::Vec3& finalServer = expected->second.position;
    const float dx = finalClient.x - finalServer.x;
    const float dy = finalClient.y - finalServer.y;
    const float dz = finalClient.z - finalServer.z;
    const float finalError = std::sqrt(dx * dx + dy * dy + dz * dz);

    std::printf(
        "move: %u ticks (%.1f s), latency %u ticks, snapshots %u applied, "
        "%u lost, inputs applied %u, corrections %u (max %.3f m), "
        "final error %.4f m\n",
        ticks, ticks * kDt, kLatencyTicks, applied, dropped, lastAppliedInput,
        corrections, maxCorrection, finalError);

    const bool ok = finalError < 0.02f && corrections >= 1 && applied > 0 &&
                    dropped >= 1 && lastAppliedInput == ticks;
    std::printf("coopnet_move: %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}

namespace {

int serveMoving(coopnet::UdpSocket& socket, std::atomic<bool>* stop) {
    std::printf("move server: listening on port %u\n", socket.localPort());
    std::fflush(stdout);

    coopnet::NetServerConfig config;
    config.peer = peerConfig();
    coopnet::NetServer server(config);
    const coopnet::SnapshotCodec codec = makeCodec();

    struct ClientState {
        coopnet::ClientReplicator replicator;
        std::uint32_t actorNetId = 0;
        std::uint32_t sentTick = 0;
        std::uint32_t ackedTick = 0;
    };
    std::map<coopnet::Endpoint, ClientState> clients;
    std::map<std::uint32_t, coopnet::PlayerActor> actors;

    const auto started = std::chrono::steady_clock::now();
    std::uint32_t tick = 0;
    std::uint32_t nextSnapshotMs = 0;

    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (stop != nullptr && stop->load()) {
            return 0;
        }
        if (now > kServerIdleTimeoutMs) {
            std::printf("move server: overall timeout\n");
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
                                      delivered.delivery.payload.size(),
                                      input)) {
                continue;
            }
            const std::uint32_t netId = server.netIdOf(delivered.from);
            if (netId == 0) {
                continue;
            }
            auto& client = clients[delivered.from];
            if (client.actorNetId == 0) {
                client.actorNetId = netId;
                actors.try_emplace(netId, netId);
            }
            auto actor = actors.find(client.actorNetId);
            if (actor != actors.end()) {
                actor->second.applyInput(input, kDt);
            }
            client.ackedTick = input.ackSnapshot;
        }

        if (now >= nextSnapshotMs) {
            nextSnapshotMs = now + kSnapshotIntervalMs;

            coopnet::Snapshot world;
            world.tick = ++tick;
            for (const auto& [netId, actor] : actors) {
                world.entities.push_back(actor.toEntity());
            }

            for (auto& [endpoint, client] : clients) {
                auto actor = actors.find(client.actorNetId);
                if (actor == actors.end()) {
                    continue;
                }
                const bool lagging =
                    client.ackedTick == 0 ||
                    tick - client.ackedTick > kMaxAckLagTicks;
                const std::uint32_t base = lagging ? 0 : client.sentTick;

                coopnet::Snapshot message = client.replicator.next(
                    world, actor->second.state().position, base);
                message.lastProcessedInput = actor->second.lastInputTick();

                std::vector<std::uint8_t> datagram;
                if (!encodeSnapshot(codec, message, datagram)) {
                    continue;
                }
                if (!server.send(endpoint, coopnet::Channel::Snapshot,
                                 datagram)) {
                    client.sentTick = 0;
                    continue;
                }
                client.sentTick = world.tick;
            }
        }

        for (const auto& outgoing : server.poll(now)) {
            socket.sendTo(outgoing.to, outgoing.data.data(),
                          outgoing.data.size());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int runMovingClient(std::uint16_t port, std::uint32_t runMs,
                    const char* tag) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("%s: cannot open socket\n", tag);
        return 1;
    }
    coopnet::ClientPeer peer(peerConfig(0xFF));
    if (!peer.start("move-client")) {
        std::printf("%s: cannot start handshake\n", tag);
        return 1;
    }

    const coopnet::Endpoint server = coopnet::Endpoint::loopback(port);
    const coopnet::SnapshotCodec codec = makeCodec();
    coopnet::SnapshotStore store;
    coopnet::ClientPredictor predictor{coopnet::MovementConfig{}, kDt};
    coopnet::InterpolationBuffer interp;
    std::deque<coopnet::InputState> recent;

    const auto started = std::chrono::steady_clock::now();
    std::uint32_t localTick = 0;
    std::uint32_t nextInputMs = 0;
    std::uint32_t ownNetId = 0;
    bool sawOwn = false;
    bool sawOther = false;
    bool haveFirst = false;
    coopnet::Vec3 firstPosition{};
    coopnet::Vec3 lastPosition{};
    float moved = 0.0f;
    bool haveRemoteSample = false;
    coopnet::Vec3 lastRemoteSample{};
    coopnet::Vec3 firstRemoteSample{};
    float remoteMoved = 0.0f;
    float maxRemoteStep = 0.0f;
    std::size_t remoteFrames = 0;
    std::size_t applied = 0;
    std::size_t rejected = 0;
    std::size_t corrections = 0;
    float maxCorrection = 0.0f;
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
        if (peer.state() == coopnet::PeerState::Joined && ownNetId == 0) {
            ownNetId = peer.netId();
        }

        if (peer.state() == coopnet::PeerState::Joined && now >= nextInputMs) {
            nextInputMs = now + kInputIntervalMs;
            const coopnet::InputState input =
                makeMoveInput(++localTick, store.tick());
            predictor.predict(input);
            recent.push_back(input);
            while (recent.size() > 3) {
                recent.pop_front();
            }
            // Redundancy against loss: each tick is on the wire three times;
            // the actor rejects stale and duplicate ticks.
            for (const auto& pending : recent) {
                peer.send(coopnet::Channel::Input,
                          coopnet::encodeInput(pending));
            }
        }

        for (const auto& delivery : peer.takeDelivered()) {
            if (delivery.channel != coopnet::Channel::Snapshot) {
                continue;
            }
            coopnet::Snapshot snapshot;
            coopnet::BitReader reader(delivery.payload.data(),
                                      delivery.payload.size());
            if (!codec.decode(reader, snapshot) || !store.apply(snapshot)) {
                ++rejected;
                continue;
            }
            ++applied;

            std::vector<coopnet::EntityState> merged;
            merged.reserve(store.entities().size());
            for (const auto& [netId, entity] : store.entities()) {
                (void)netId;
                merged.push_back(entity);
            }
            interp.push(store.tick(), now, std::move(merged));

            if (ownNetId != 0) {
                const coopnet::EntityState* own = store.find(ownNetId);
                if (own != nullptr) {
                    coopnet::MoveState authoritative;
                    authoritative.position = own->position;
                    authoritative.velocity = own->velocity;
                    authoritative.onGround = (own->stateFlags & 1) != 0;
                    const std::uint32_t ack = store.lastProcessedInput();
                    if (ack == 0) {
                        predictor.reconcile(authoritative, 0);
                    } else {
                        const float error =
                            predictor.reconcile(authoritative, ack);
                        if (ack > 60) {
                            if (error > 1e-3f) {
                                ++corrections;
                            }
                            maxCorrection =
                                std::max(maxCorrection, error);
                        }
                    }
                    sawOwn = true;
                    lastPosition = own->position;
                    if (!haveFirst) {
                        firstPosition = own->position;
                        haveFirst = true;
                    }
                    const float dx = lastPosition.x - firstPosition.x;
                    const float dy = lastPosition.y - firstPosition.y;
                    const float dz = lastPosition.z - firstPosition.z;
                    moved = std::max(moved, std::sqrt(dx * dx + dy * dy +
                                                      dz * dz));
                }
            }

            for (const auto& [netId, entity] : store.entities()) {
                (void)entity;
                if (netId != ownNetId) {
                    sawOther = true;
                }
            }
        }

        if (ownNetId != 0 && interp.size() > 0) {
            std::vector<coopnet::EntityState> sampled;
            interp.sample(now, 100, sampled);
            for (const auto& entity : sampled) {
                if (entity.netId == ownNetId) {
                    continue;
                }
                ++remoteFrames;
                if (!haveRemoteSample) {
                    firstRemoteSample = entity.position;
                    lastRemoteSample = entity.position;
                    haveRemoteSample = true;
                } else {
                    const float dx = entity.position.x - lastRemoteSample.x;
                    const float dy = entity.position.y - lastRemoteSample.y;
                    const float dz = entity.position.z - lastRemoteSample.z;
                    maxRemoteStep = std::max(
                        maxRemoteStep, std::sqrt(dx * dx + dy * dy + dz * dz));
                    lastRemoteSample = entity.position;
                }
                const float rx = entity.position.x - firstRemoteSample.x;
                const float ry = entity.position.y - firstRemoteSample.y;
                const float rz = entity.position.z - firstRemoteSample.z;
                remoteMoved = std::max(
                    remoteMoved, std::sqrt(rx * rx + ry * ry + rz * rz));
                sawOther = true;
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto& stats = peer.session().stats();
    std::printf(
        "%s: net %u, snapshots %zu (rejected %zu), own actor %s, other %s, "
        "moved %.2f m, corrections %zu (max %.3f m), "
        "remote interp %zu frames, moved %.2f m, max step %.3f m, "
        "rtt %u ms, tx %llu B, rx %llu B\n",
        tag, ownNetId, applied, rejected, sawOwn ? "seen" : "missing",
        sawOther ? "seen" : "missing", moved, corrections, maxCorrection,
        remoteFrames, remoteMoved, maxRemoteStep,
        peer.session().rttMs(),
        static_cast<unsigned long long>(stats.sentBytes),
        static_cast<unsigned long long>(stats.receivedBytes));
    std::fflush(stdout);

    if (applied > 0 && rejected <= 2 && sawOwn && sawOther && moved > 2.0f &&
        maxCorrection < 0.15f && remoteFrames > 30 && remoteMoved > 1.0f &&
        maxRemoteStep < 0.5f) {
        result = 0;
    }
    return result;
}

int runTwo(std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("move: cannot open server socket\n");
        return 1;
    }

    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&socket, &stop, &serverResult] {
        serverResult = serveMoving(socket, &stop);
    });

    int firstResult = 1;
    int secondResult = 1;
    std::thread firstThread([&socket, &firstResult, runMs] {
        firstResult =
            runMovingClient(socket.localPort(), runMs, "move client 1");
    });
    std::thread secondThread([&socket, &secondResult, runMs] {
        secondResult =
            runMovingClient(socket.localPort(), runMs, "move client 2");
    });

    firstThread.join();
    secondThread.join();
    stop.store(true);
    serverThread.join();

    if (firstResult == 0 && secondResult == 0 && serverResult == 0) {
        std::printf("coopnet_move: PASSED (two clients)\n");
        return 0;
    }
    std::printf("coopnet_move: FAILED (client 1 %d, client 2 %d, server %d)\n",
                firstResult, secondResult, serverResult);
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("coopnet_move: network init failed\n");
        return 1;
    }

    if (argc < 2) {
        std::printf("usage:\n  coopnet_move local [ticks]\n"
                    "  coopnet_move two [ms]\n");
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "local") {
        std::uint32_t ticks = kDefaultTicks;
        if (argc > 2 && !parseTicks(argv[2], ticks)) {
            std::printf("coopnet_move: bad tick count\n");
            return 2;
        }
        return runLocal(ticks);
    }
    if (mode == "two") {
        std::uint32_t runMs = kDefaultRunMs;
        if (argc > 2 && !parseMs(argv[2], runMs)) {
            std::printf("coopnet_move: bad duration\n");
            return 2;
        }
        return runTwo(runMs);
    }

    std::printf("coopnet_move: unknown mode '%s'\n", mode.c_str());
    return 2;
}