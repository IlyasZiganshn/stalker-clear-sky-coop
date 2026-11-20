// Application messages (chat) over coopnet: a server rebroadcasts each message
// to every joined peer except its author.
//
//   coopnet_chat local [ms]
//   coopnet_chat server [port]
//   coopnet_chat client <port> <name> [ms]

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "coopnet/app_message.h"
#include "coopnet/net.h"
#include "coopnet/peer.h"
#include "coopnet/server.h"

namespace {

constexpr std::uint32_t kBuildHash = 0xB0B0B0B0u;
constexpr std::uint16_t kDefaultPort = 27026;
constexpr std::uint32_t kServerIdleTimeoutMs = 60000;
constexpr std::uint32_t kLocalRunMs = 3000;
constexpr std::uint32_t kSendIntervalMs = 300;
constexpr std::size_t kMessagesPerClient = 5;

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

int serveChat(coopnet::UdpSocket& socket, std::atomic<bool>* stop = nullptr) {
    coopnet::NetServerConfig config;
    config.peer = serverPeerConfig();
    coopnet::NetServer server(config);

    const auto started = std::chrono::steady_clock::now();
    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (stop != nullptr && stop->load()) {
            return 0;
        }
        if (now > kServerIdleTimeoutMs) {
            std::printf("chat server: overall timeout\n");
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
            coopnet::AppMessage message;
            if (!coopnet::decodeAppMessage(delivery.delivery.payload.data(),
                                           delivery.delivery.payload.size(),
                                           message)) {
                continue;
            }
            message.sender = server.netIdOf(delivery.from);
            if (message.sender == 0) {
                continue;
            }
            std::vector<std::uint8_t> wire;
            if (!coopnet::encodeAppMessage(message, wire)) {
                continue;
            }
            server.broadcastExcept(delivery.from, coopnet::Channel::Control,
                                   wire);
        }

        for (const auto& outgoing : server.poll(now)) {
            socket.sendTo(outgoing.to, outgoing.data.data(),
                          outgoing.data.size());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct ChatClientStats {
    std::uint32_t netId = 0;
    std::size_t sent = 0;
    std::size_t received = 0;
    std::size_t malformed = 0;
    std::vector<std::pair<std::uint32_t, std::string>> messages;
};

// Sends `kMessagesPerClient` numbered chats once joined and collects everything
// the server rebroadcasts.
int runChatClient(std::uint16_t port, const std::string& name,
                  std::uint32_t runMs, bool printMessages,
                  ChatClientStats& stats) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("chat client %s: cannot open socket\n", name.c_str());
        return 1;
    }
    coopnet::ClientPeer peer(clientPeerConfig());
    if (!peer.start(name)) {
        std::printf("chat client %s: cannot start handshake\n", name.c_str());
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
            std::printf("chat client %s: denied (reason %u)\n", name.c_str(),
                        static_cast<unsigned>(peer.denyReason()));
            break;
        }

        if (peer.state() == coopnet::PeerState::Joined) {
            stats.netId = peer.netId();
            if (stats.sent < kMessagesPerClient && now >= nextSendMs) {
                nextSendMs = now + kSendIntervalMs;
                coopnet::AppMessage message;
                message.text = name + " " + std::to_string(stats.sent + 1);
                std::vector<std::uint8_t> wire;
                if (coopnet::encodeAppMessage(message, wire) &&
                    peer.send(coopnet::Channel::Control, wire)) {
                    ++stats.sent;
                }
            }
        }

        for (const auto& delivery : peer.takeDelivered()) {
            if (delivery.channel != coopnet::Channel::Control) {
                continue;
            }
            coopnet::AppMessage message;
            if (!coopnet::decodeAppMessage(delivery.payload.data(),
                                           delivery.payload.size(), message)) {
                ++stats.malformed;
                continue;
            }
            ++stats.received;
            stats.messages.emplace_back(message.sender, message.text);
            if (printMessages) {
                std::printf("chat client %s: <%u> %s\n", name.c_str(),
                            message.sender, message.text.c_str());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (peer.state() == coopnet::PeerState::Joined || stats.sent > 0) {
        result = 0;
    }
    std::printf(
        "chat client %s: net %u, sent %zu, received %zu, malformed %zu, "
        "rtt %u ms\n",
        name.c_str(), stats.netId, stats.sent, stats.received, stats.malformed,
        peer.session().rttMs());
    std::fflush(stdout);
    return result;
}

bool messagesMatch(const ChatClientStats& stats, std::uint32_t otherNetId,
                   const std::string& otherName) {
    if (stats.messages.size() != kMessagesPerClient) {
        return false;
    }
    for (std::size_t i = 0; i < stats.messages.size(); ++i) {
        if (stats.messages[i].first != otherNetId) {
            return false;
        }
        if (stats.messages[i].second !=
            otherName + " " + std::to_string(i + 1)) {
            return false;
        }
    }
    return true;
}

int runLocal(std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open()) {
        std::printf("chat: cannot open server socket\n");
        return 1;
    }

    std::atomic<bool> stop{false};
    int serverResult = 1;
    std::thread serverThread([&socket, &stop, &serverResult] {
        serverResult = serveChat(socket, &stop);
    });

    ChatClientStats first;
    ChatClientStats second;
    int firstResult = 1;
    int secondResult = 1;
    std::thread firstThread([&socket, &first, &firstResult, runMs] {
        firstResult =
            runChatClient(socket.localPort(), "alpha", runMs, false, first);
    });
    std::thread secondThread([&socket, &second, &secondResult, runMs] {
        secondResult =
            runChatClient(socket.localPort(), "bravo", runMs, false, second);
    });

    firstThread.join();
    secondThread.join();
    stop.store(true);
    serverThread.join();

    const bool firstOk = firstResult == 0 &&
        messagesMatch(first, second.netId, "bravo");
    const bool secondOk = secondResult == 0 &&
        messagesMatch(second, first.netId, "alpha");

    if (firstOk && secondOk && serverResult == 0) {
        std::printf("coopnet_chat: PASSED\n");
        return 0;
    }
    std::printf(
        "coopnet_chat: FAILED (alpha %d, bravo %d, server %d, "
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

}  // namespace

int main(int argc, char** argv) {
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("coopnet_chat: network init failed\n");
        return 1;
    }

    if (argc < 2) {
        std::printf("usage:\n"
                    "  coopnet_chat local [ms]\n"
                    "  coopnet_chat server [port]\n"
                    "  coopnet_chat client <port> <name> [ms]\n");
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "local") {
        std::uint32_t runMs = kLocalRunMs;
        if (argc > 2 && !parseMs(argv[2], runMs)) {
            std::printf("coopnet_chat: bad duration\n");
            return 2;
        }
        return runLocal(runMs);
    }
    if (mode == "server") {
        std::uint16_t port = kDefaultPort;
        if (argc > 2 && !parsePort(argv[2], port)) {
            std::printf("coopnet_chat: bad port\n");
            return 2;
        }
        coopnet::UdpSocket socket;
        if (!socket.open(port)) {
            std::printf("chat server: cannot bind port %u\n", port);
            return 1;
        }
        return serveChat(socket);
    }
    if (mode == "client") {
        std::uint16_t port = 0;
        if (argc < 4 || !parsePort(argv[2], port)) {
            std::printf("coopnet_chat: bad port or name\n");
            return 2;
        }
        std::uint32_t runMs = kLocalRunMs;
        if (argc > 4 && !parseMs(argv[4], runMs)) {
            std::printf("coopnet_chat: bad duration\n");
            return 2;
        }
        ChatClientStats stats;
        return runChatClient(port, argv[3], runMs, true, stats);
    }

    std::printf("coopnet_chat: unknown mode '%s'\n", mode.c_str());
    return 2;
}