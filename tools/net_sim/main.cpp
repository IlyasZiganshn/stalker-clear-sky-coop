// Network condition simulator: a UDP relay with one-way delay and packet loss.
//
//   net_sim relay <listenPort> <host:port> [delayMs] [lossPercent] [runMs]
//
// A client configured to talk to <listenPort> sees the server through the
// relay: every datagram is delivered after delayMs, and lossPercent of them are
// dropped independently in each direction. Used to measure prediction and
// delta recovery under conditions close to the M1 acceptance target.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "coopnet/net.h"

namespace {

constexpr std::uint32_t kDefaultDelayMs = 40;  // 80 ms RTT
constexpr std::uint32_t kDefaultRunMs = 0;     // 0 = until killed
constexpr std::uint32_t kMaxDatagram = 2048;

std::uint32_t elapsedMs(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

struct Lcg {
    std::uint32_t state = 0x2545F491u;

    std::uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    bool drop(std::uint32_t percent) {
        return percent != 0 && next() % 100u < percent;
    }
};

bool parseU32(const char* text, std::uint32_t& out) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool parseEndpoint(const std::string& text, coopnet::Endpoint& out) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string::npos) {
        return false;
    }
    std::uint32_t port = 0;
    if (!parseU32(text.c_str() + colon + 1, port) || port == 0 ||
        port > 65535) {
        return false;
    }
    return coopnet::Endpoint::parse(text.substr(0, colon),
                                   static_cast<std::uint16_t>(port), out);
}

struct Pending {
    coopnet::Endpoint to;
    std::vector<std::uint8_t> data;
};

int runRelay(std::uint16_t listenPort, const coopnet::Endpoint& server,
             std::uint32_t delayMs, std::uint32_t lossPercent,
             std::uint32_t runMs) {
    coopnet::UdpSocket socket;
    if (!socket.open(listenPort)) {
        std::printf("net_sim: cannot bind port %u\n", listenPort);
        return 1;
    }

    std::printf(
        "net_sim: relay %u -> %s, delay %u ms one way, loss %u%%\n",
        socket.localPort(), server.toString().c_str(), delayMs, lossPercent);
    std::fflush(stdout);

    const auto started = std::chrono::steady_clock::now();
    coopnet::Endpoint client{};
    bool haveClient = false;
    std::multimap<std::uint32_t, Pending> queue;
    Lcg rng;
    std::uint64_t toServer = 0;
    std::uint64_t toClient = 0;
    std::uint64_t dropped = 0;

    for (;;) {
        const std::uint32_t now = elapsedMs(started);
        if (runMs != 0 && now > runMs) {
            break;
        }

        std::uint8_t buffer[kMaxDatagram];
        for (;;) {
            coopnet::Endpoint from;
            const std::int32_t received =
                socket.receiveFrom(buffer, sizeof(buffer), from);
            if (received < 0) {
                break;
            }
            if (from == server) {
                if (!haveClient) {
                    continue;
                }
                if (rng.drop(lossPercent)) {
                    ++dropped;
                    continue;
                }
                Pending pending{client,
                                std::vector<std::uint8_t>(
                                    buffer, buffer + received)};
                queue.emplace(now + delayMs, std::move(pending));
                ++toClient;
            } else {
                if (!haveClient) {
                    client = from;
                    haveClient = true;
                    std::printf("net_sim: client is %s\n",
                                client.toString().c_str());
                    std::fflush(stdout);
                }
                if (from != client) {
                    continue;
                }
                if (rng.drop(lossPercent)) {
                    ++dropped;
                    continue;
                }
                Pending pending{server,
                                std::vector<std::uint8_t>(
                                    buffer, buffer + received)};
                queue.emplace(now + delayMs, std::move(pending));
                ++toServer;
            }
        }

        while (!queue.empty() && queue.begin()->first <= now) {
            const Pending& pending = queue.begin()->second;
            socket.sendTo(pending.to, pending.data.data(), pending.data.size());
            queue.erase(queue.begin());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::printf("net_sim: forwarded %llu to server, %llu to client, dropped %llu\n",
                static_cast<unsigned long long>(toServer),
                static_cast<unsigned long long>(toClient),
                static_cast<unsigned long long>(dropped));
    std::fflush(stdout);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("net_sim: network init failed\n");
        return 1;
    }

    if (argc < 4 || std::strcmp(argv[1], "relay") != 0) {
        std::printf(
            "usage:\n"
            "  net_sim relay <listenPort> <host:port> [delayMs] "
            "[lossPercent] [runMs]\n");
        return 2;
    }

    std::uint32_t listenPort = 0;
    if (!parseU32(argv[2], listenPort) || listenPort == 0 ||
        listenPort > 65535) {
        std::printf("net_sim: bad listen port\n");
        return 2;
    }
    coopnet::Endpoint server;
    if (!parseEndpoint(argv[3], server)) {
        std::printf("net_sim: bad server endpoint\n");
        return 2;
    }
    std::uint32_t delayMs = kDefaultDelayMs;
    std::uint32_t lossPercent = 0;
    std::uint32_t runMs = kDefaultRunMs;
    if ((argc > 4 && !parseU32(argv[4], delayMs)) ||
        (argc > 5 && (!parseU32(argv[5], lossPercent) || lossPercent > 100)) ||
        (argc > 6 && !parseU32(argv[6], runMs))) {
        std::printf("net_sim: bad relay parameters\n");
        return 2;
    }

    return runRelay(static_cast<std::uint16_t>(listenPort), server, delayMs,
                    lossPercent, runMs);
}