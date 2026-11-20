// End-to-end check of coopnet over real UDP sockets on loopback.
// Two sessions talk through the OS network stack with injected packet loss.
// Not a game test: proves transport framing, reliability and reassembly.

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "coopnet/session.h"

namespace {

constexpr int kMessageCount = 40;
constexpr int kLossPercent = 30;
constexpr int kRunLimitMs = 15000;
constexpr std::size_t kLargeEvery = 7;
constexpr std::size_t kLargeSize = 4000;

std::uint32_t g_state = 0x1234ABCDu;

std::uint32_t nextRandom() {
    g_state = g_state * 1664525u + 1013904223u;
    return g_state;
}

bool lost() {
    return nextRandom() % 100u < static_cast<std::uint32_t>(kLossPercent);
}

struct Endpoint {
    SOCKET socket = INVALID_SOCKET;
    sockaddr_in address{};

    bool open() {
        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket == INVALID_SOCKET) {
            return false;
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (::bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            return false;
        }
        int length = sizeof(address);
        if (::getsockname(socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
            return false;
        }
        u_long nonBlocking = 1;
        return ::ioctlsocket(socket, FIONBIO, &nonBlocking) == 0;
    }

    void close() {
        if (socket != INVALID_SOCKET) {
            ::closesocket(socket);
            socket = INVALID_SOCKET;
        }
    }
};

void transmit(const Endpoint& from, const Endpoint& to,
              const std::vector<std::vector<std::uint8_t>>& datagrams) {
    for (const auto& datagram : datagrams) {
        if (lost()) {
            continue;
        }
        ::sendto(from.socket, reinterpret_cast<const char*>(datagram.data()),
                 static_cast<int>(datagram.size()), 0,
                 reinterpret_cast<const sockaddr*>(&to.address), sizeof(to.address));
    }
}

void receive(const Endpoint& endpoint, coopnet::Session& session, std::uint32_t nowMs) {
    std::uint8_t buffer[2048];
    for (;;) {
        sockaddr_in from{};
        int fromLength = sizeof(from);
        const int received =
            ::recvfrom(endpoint.socket, reinterpret_cast<char*>(buffer),
                       static_cast<int>(sizeof(buffer)), 0,
                       reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (received <= 0) {
            return;
        }
        session.onDatagram(buffer, static_cast<std::size_t>(received), nowMs);
    }
}

std::string payloadText(std::size_t index) {
    return "coopnet-selftest-message-" + std::to_string(index);
}

std::vector<std::uint8_t> makePayload(std::size_t index) {
    if (index % kLargeEvery == 0) {
        std::vector<std::uint8_t> payload(kLargeSize);
        for (std::size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<std::uint8_t>((index * 31 + i * 7) & 0xFF);
        }
        return payload;
    }
    const std::string text = payloadText(index);
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

}  // namespace

int main() {
    WSADATA wsa{};
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::printf("selftest: WSAStartup failed\n");
        return 1;
    }

    Endpoint clientEndpoint;
    Endpoint serverEndpoint;
    if (!clientEndpoint.open() || !serverEndpoint.open()) {
        std::printf("selftest: cannot open UDP sockets\n");
        return 1;
    }

    coopnet::Session client;
    coopnet::Session server;
    std::vector<coopnet::Session::Delivery> received;
    std::size_t nextToSend = 0;

    const auto started = std::chrono::steady_clock::now();
    std::uint32_t nowMs = 0;
    bool ok = false;

    while (nowMs < kRunLimitMs) {
        nowMs = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started)
                .count());

        while (nextToSend < kMessageCount) {
            if (!client.send(coopnet::Channel::Control, makePayload(nextToSend))) {
                break;
            }
            ++nextToSend;
        }

        transmit(clientEndpoint, serverEndpoint, client.poll(nowMs));
        receive(serverEndpoint, server, nowMs);
        transmit(serverEndpoint, clientEndpoint, server.poll(nowMs));
        receive(clientEndpoint, client, nowMs);

        auto delivered = server.takeDelivered();
        for (auto& delivery : delivered) {
            received.push_back(std::move(delivery));
        }

        if (nextToSend == kMessageCount && delivered.empty() &&
            client.inFlight(coopnet::Channel::Control) == 0 &&
            received.size() == kMessageCount) {
            ok = true;
            break;
        }

        ::Sleep(1);
    }

    bool orderOk = received.size() == static_cast<std::size_t>(kMessageCount);
    for (std::size_t i = 0; orderOk && i < received.size(); ++i) {
        const auto expected = makePayload(i);
        if (received[i].payload == expected) {
            continue;
        }
        orderOk = false;
        std::printf("selftest: mismatch at %zu: got %zu bytes, expected %zu bytes\n", i,
                    received[i].payload.size(), expected.size());
        for (std::size_t j = 0; j < kMessageCount; ++j) {
            if (received[i].payload == makePayload(j)) {
                std::printf("selftest: delivery %zu actually holds message %zu\n", i, j);
                break;
            }
        }
    }

    std::printf(
        "selftest: %zu/%d messages, order %s, %llu datagrams sent, %llu received, "
        "%llu dropped, %llu malformed, %u ms, loss injected %d%%\n",
        received.size(), kMessageCount, orderOk ? "ok" : "BROKEN",
        static_cast<unsigned long long>(client.stats().sentDatagrams +
                                        server.stats().sentDatagrams),
        static_cast<unsigned long long>(client.stats().receivedDatagrams +
                                        server.stats().receivedDatagrams),
        static_cast<unsigned long long>(server.stats().droppedDatagrams),
        static_cast<unsigned long long>(client.stats().malformed +
                                        server.stats().malformed),
        nowMs, kLossPercent);

    clientEndpoint.close();
    serverEndpoint.close();
    ::WSACleanup();

    if (!ok || !orderOk) {
        std::printf("selftest: FAILED\n");
        return 1;
    }
    std::printf("selftest: PASSED\n");
    return 0;
}