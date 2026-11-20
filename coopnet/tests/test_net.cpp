#include "coopnet/net.h"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "test_framework.h"

using coopnet::Endpoint;
using coopnet::NetworkScope;
using coopnet::UdpSocket;

namespace {

bool receiveSome(UdpSocket& socket, std::vector<std::uint8_t>& out,
                 Endpoint& from) {
    std::uint8_t buffer[512];
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
        const std::int32_t size = socket.receiveFrom(buffer, sizeof(buffer), from);
        if (size > 0) {
            out.assign(buffer, buffer + size);
            return true;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

}  // namespace

COOP_TEST(net_loopback_roundtrip) {
    NetworkScope scope;
    CHECK(scope.ok());

    UdpSocket sender;
    UdpSocket receiver;
    CHECK(sender.open());
    CHECK(receiver.open());
    CHECK(sender.valid());
    CHECK(receiver.valid());
    CHECK(sender.localPort() != 0);
    CHECK(receiver.localPort() != 0);

    const std::uint8_t payload[] = {1, 2, 3, 4, 5};
    CHECK(sender.sendTo(Endpoint::loopback(receiver.localPort()), payload,
                        sizeof(payload)));

    std::vector<std::uint8_t> received;
    Endpoint from;
    CHECK(receiveSome(receiver, received, from));
    CHECK(received == std::vector<std::uint8_t>(payload, payload + sizeof(payload)));
    CHECK_EQ(from.port, sender.localPort());
}

COOP_TEST(net_idle_socket_reports_nothing) {
    NetworkScope scope;
    UdpSocket socket;
    CHECK(socket.open());

    std::uint8_t buffer[16];
    Endpoint from;
    CHECK_EQ(socket.receiveFrom(buffer, sizeof(buffer), from), std::int32_t{-1});
}

COOP_TEST(net_endpoint_formatting) {
    const Endpoint endpoint = Endpoint::loopback(27015);
    CHECK(endpoint.toString() == "127.0.0.1:27015");
    CHECK(Endpoint::loopback(27015) == endpoint);
}

COOP_TEST(net_endpoint_parsing) {
    Endpoint endpoint;
    CHECK(Endpoint::parse("192.168.0.10", 27015, endpoint));
    CHECK(endpoint.toString() == "192.168.0.10:27015");

    CHECK(!Endpoint::parse("localhost", 27015, endpoint));
    CHECK(!Endpoint::parse("192.168.0.300", 27015, endpoint));
    CHECK(!Endpoint::parse("", 27015, endpoint));
}

COOP_TEST(net_move_transfers_the_socket) {
    NetworkScope scope;
    UdpSocket first;
    CHECK(first.open());
    const std::uint16_t port = first.localPort();
    CHECK(port != 0);

    UdpSocket second(std::move(first));
    CHECK(!first.valid());
    CHECK(second.valid());
    CHECK_EQ(second.localPort(), port);
}

COOP_TEST(net_closed_socket_refuses_io) {
    UdpSocket socket;
    CHECK(!socket.valid());

    const std::uint8_t byte = 0;
    CHECK(!socket.sendTo(Endpoint::loopback(1), &byte, 1));

    std::uint8_t buffer[4];
    Endpoint from;
    CHECK_EQ(socket.receiveFrom(buffer, sizeof(buffer), from), std::int32_t{-1});
    CHECK_EQ(socket.localPort(), std::uint16_t{0});
}