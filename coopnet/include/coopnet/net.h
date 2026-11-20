#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace coopnet {

// IPv4 endpoint in host byte order.
struct Endpoint {
    std::uint32_t address = 0x7F000001u;  // 127.0.0.1
    std::uint16_t port = 0;

    static Endpoint loopback(std::uint16_t port = 0);
    std::string toString() const;

    // Numeric IPv4 only ("127.0.0.1"); no DNS. Returns false on bad input.
    static bool parse(const std::string& host, std::uint16_t port,
                      Endpoint& out);

    bool operator==(const Endpoint&) const = default;
    bool operator<(const Endpoint& other) const {
        if (address != other.address) {
            return address < other.address;
        }
        return port < other.port;
    }
};

// Winsock init/teardown on Windows, no-op elsewhere. Create one before opening
// sockets and keep it alive for as long as they are used.
class NetworkScope {
public:
    NetworkScope();
    ~NetworkScope();

    NetworkScope(const NetworkScope&) = delete;
    NetworkScope& operator=(const NetworkScope&) = delete;

    bool ok() const { return ok_; }

private:
    bool ok_ = true;
};

// Non-blocking IPv4 UDP socket. One socket carries all coopnet channels.
class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // Binds to all interfaces; `port` 0 picks a free one.
    bool open();
    bool open(std::uint16_t port);
    void close();

    bool valid() const { return handle_ != kInvalidHandle; }
    std::uint16_t localPort() const;

    bool sendTo(const Endpoint& to, const void* data, std::size_t size);

    // Bytes received, or -1 when nothing is pending.
    std::int32_t receiveFrom(void* buffer, std::size_t capacity, Endpoint& from);

private:
    static constexpr std::intptr_t kInvalidHandle = -1;

    std::intptr_t handle_ = kInvalidHandle;
};

}  // namespace coopnet