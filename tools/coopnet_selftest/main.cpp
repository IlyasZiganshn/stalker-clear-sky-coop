// End-to-end check of coopnet over real UDP sockets on loopback.
// Two sessions talk through the OS network stack with injected packet loss.
// Not a game test: proves transport framing, reliability and reassembly.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "coopnet/net.h"
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

void transmit(coopnet::UdpSocket& from, const coopnet::Endpoint& to,
              const std::vector<std::vector<std::uint8_t>>& datagrams) {
    for (const auto& datagram : datagrams) {
        if (lost()) {
            continue;
        }
        from.sendTo(to, datagram.data(), datagram.size());
    }
}

void receive(coopnet::UdpSocket& socket, coopnet::Session& session,
             std::uint32_t nowMs) {
    std::uint8_t buffer[2048];
    coopnet::Endpoint from;
    for (;;) {
        const std::int32_t received = socket.receiveFrom(buffer, sizeof(buffer), from);
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
    coopnet::NetworkScope network;
    if (!network.ok()) {
        std::printf("selftest: network init failed\n");
        return 1;
    }

    coopnet::UdpSocket clientSocket;
    coopnet::UdpSocket serverSocket;
    if (!clientSocket.open() || !serverSocket.open()) {
        std::printf("selftest: cannot open UDP sockets\n");
        return 1;
    }
    const coopnet::Endpoint clientEndpoint =
        coopnet::Endpoint::loopback(clientSocket.localPort());
    const coopnet::Endpoint serverEndpoint =
        coopnet::Endpoint::loopback(serverSocket.localPort());

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

        transmit(clientSocket, serverEndpoint, client.poll(nowMs));
        receive(serverSocket, server, nowMs);
        transmit(serverSocket, clientEndpoint, server.poll(nowMs));
        receive(clientSocket, client, nowMs);

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

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

    if (!ok || !orderOk) {
        std::printf("selftest: FAILED\n");
        return 1;
    }
    std::printf("selftest: PASSED\n");
    return 0;
}