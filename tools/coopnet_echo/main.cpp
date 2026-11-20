// Runnable smoke test of coopnet over real UDP: handshake, reliable events and
// echo. Proves the transport layer without the game.
//
//   coopnet_echo local                 server thread + client in one process
//   coopnet_echo server [port]         stand-alone server
//   coopnet_echo client <port> [name]  stand-alone client

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/session.h"

namespace {

constexpr std::uint32_t kBuildHash = 0xC0FFEEu;
constexpr std::size_t kMessageCount = 20;
constexpr std::uint16_t kDefaultPort = 27015;
constexpr std::uint32_t kClientTimeoutMs = 20000;
constexpr std::uint32_t kServerTimeoutMs = 60000;

std::uint32_t elapsedMs(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

coopnet::PeerConfig serverConfig() {
    coopnet::PeerConfig config;
    config.buildHash = kBuildHash;
    config.sessionToken = 0x5EED0001u;
    config.maxPlayers = 4;
    return config;
}

coopnet::PeerConfig clientConfig() {
    coopnet::PeerConfig config;
    config.buildHash = kBuildHash;
    return config;
}

std::vector<std::uint8_t> messagePayload(std::size_t index) {
    const std::string text = "echo-" + std::to_string(index);
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

void printTraffic(const char* who, const coopnet::Session& session) {
    const auto& stats = session.stats();
    std::printf(
        "%s: rtt %u ms, sent %llu datagrams / %llu B, received %llu / %llu B, "
        "malformed %llu\n",
        who, session.rttMs(),
        static_cast<unsigned long long>(stats.sentDatagrams),
        static_cast<unsigned long long>(stats.sentBytes),
        static_cast<unsigned long long>(stats.receivedDatagrams),
        static_cast<unsigned long long>(stats.receivedBytes),
        static_cast<unsigned long long>(stats.malformed));
    std::fflush(stdout);
}

int serve(coopnet::UdpSocket& socket, std::atomic<bool>& stop) {
    coopnet::ServerPeer peer(serverConfig());
    coopnet::Endpoint client;
    bool haveClient = false;
    bool sawJoin = false;
    const auto started = std::chrono::steady_clock::now();

    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (stop.load()) {
            return 0;
        }
        if (now > kServerTimeoutMs) {
            std::printf("echo server: overall timeout\n");
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
            client = from;
            haveClient = true;
            peer.onDatagram(buffer, static_cast<std::size_t>(received), now);
        }

        if (haveClient) {
            for (const auto& datagram : peer.poll(now)) {
                socket.sendTo(client, datagram.data(), datagram.size());
            }
        }

        if (peer.state() == coopnet::PeerState::Joined && !sawJoin) {
            sawJoin = true;
            std::printf("echo server: %s joined, net id %u\n",
                        peer.playerName().c_str(), peer.netId());
            std::fflush(stdout);
        }
        for (auto& delivery : peer.takeDelivered()) {
            if (delivery.channel == coopnet::Channel::Events) {
                peer.send(coopnet::Channel::Events, delivery.payload);
            }
        }
        if (sawJoin && peer.timedOut(now)) {
            std::printf("echo server: client gone\n");
            printTraffic("echo server", peer.session());
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int runClient(std::uint16_t port, const std::string& name) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("echo client: cannot open socket\n");
        return 1;
    }
    coopnet::ClientPeer peer(clientConfig());
    if (!peer.start(name)) {
        std::printf("echo client: cannot start handshake\n");
        return 1;
    }

    const coopnet::Endpoint server = coopnet::Endpoint::loopback(port);
    const auto started = std::chrono::steady_clock::now();
    std::size_t sent = 0;
    std::size_t received = 0;
    std::uint32_t nextSendMs = 0;
    int result = 1;

    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (now > kClientTimeoutMs) {
            std::printf("echo client: timed out\n");
            break;
        }

        for (const auto& datagram : peer.poll(now)) {
            socket.sendTo(server, datagram.data(), datagram.size());
        }

        std::uint8_t buffer[2048];
        for (;;) {
            coopnet::Endpoint from;
            const std::int32_t receivedBytes =
                socket.receiveFrom(buffer, sizeof(buffer), from);
            if (receivedBytes < 0) {
                break;
            }
            peer.onDatagram(buffer, static_cast<std::size_t>(receivedBytes), now);
        }

        if (peer.state() == coopnet::PeerState::Denied) {
            std::printf("echo client: denied (%d)\n",
                        static_cast<int>(peer.denyReason()));
            break;
        }
        if (peer.state() == coopnet::PeerState::Joined && sent < kMessageCount &&
            now >= nextSendMs) {
            if (peer.send(coopnet::Channel::Events, messagePayload(sent))) {
                ++sent;
                nextSendMs = now + 5;
            }
        }

        bool mismatch = false;
        for (const auto& delivery : peer.takeDelivered()) {
            if (delivery.payload != messagePayload(received)) {
                std::printf("echo client: echo %zu mismatch (%zu bytes)\n",
                            received, delivery.payload.size());
                mismatch = true;
                break;
            }
            ++received;
        }
        if (mismatch) {
            break;
        }
        if (received == kMessageCount) {
            result = 0;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::printf("echo client: %zu/%zu echoes, state %d\n", received, kMessageCount,
                static_cast<int>(peer.state()));
    printTraffic("echo client", peer.session());
    return result;
}

int runLocal() {
    coopnet::UdpSocket serverSocket;
    if (!serverSocket.open()) {
        std::printf("coopnet_echo: cannot open server socket\n");
        return 1;
    }

    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&serverSocket, &stop, &serverResult] {
        serverResult = serve(serverSocket, stop);
    });

    const int clientResult = runClient(serverSocket.localPort(), "tester");
    stop.store(true);
    serverThread.join();

    if (clientResult == 0 && serverResult == 0) {
        std::printf("coopnet_echo: PASSED\n");
        return 0;
    }
    std::printf("coopnet_echo: FAILED (client %d, server %d)\n", clientResult,
                serverResult);
    return 1;
}

int runServer(std::uint16_t port) {
    coopnet::UdpSocket socket;
    if (!socket.open(port)) {
        std::printf("echo server: cannot bind port %u\n", port);
        return 1;
    }
    std::printf("echo server: listening on port %u\n", socket.localPort());
    std::fflush(stdout);

    std::atomic<bool> stop{false};
    return serve(socket, stop);
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

}  // namespace

int main(int argc, char** argv) {
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("coopnet_echo: network init failed\n");
        return 1;
    }

    if (argc < 2) {
        std::printf(
            "usage:\n"
            "  coopnet_echo local\n"
            "  coopnet_echo server [port]\n"
            "  coopnet_echo client <port> [name]\n");
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "local") {
        return runLocal();
    }
    if (mode == "server") {
        std::uint16_t port = kDefaultPort;
        if (argc > 2 && !parsePort(argv[2], port)) {
            std::printf("coopnet_echo: bad port\n");
            return 2;
        }
        return runServer(port);
    }
    if (mode == "client") {
        std::uint16_t port = 0;
        if (argc < 3 || !parsePort(argv[2], port)) {
            std::printf("coopnet_echo: client needs a port\n");
            return 2;
        }
        const std::string name = argc > 3 ? argv[3] : "player";
        return runClient(port, name);
    }

    std::printf("coopnet_echo: unknown mode '%s'\n", mode.c_str());
    return 2;
}