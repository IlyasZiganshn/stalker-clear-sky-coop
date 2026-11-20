#include "coopnet/net.h"

#include <algorithm>
#include <climits>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace coopnet {

#if defined(_WIN32)
using SocketHandle = SOCKET;
using AddressLength = int;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

void closeHandle(SocketHandle handle) { ::closesocket(handle); }
#else
using SocketHandle = int;
using AddressLength = socklen_t;
constexpr SocketHandle kInvalidSocket = -1;

void closeHandle(SocketHandle handle) { ::close(handle); }
#endif

namespace {

sockaddr_in toNative(const Endpoint& endpoint) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(endpoint.address);
    address.sin_port = htons(endpoint.port);
    return address;
}

}  // namespace

Endpoint Endpoint::loopback(std::uint16_t port) {
    Endpoint endpoint;
    endpoint.address = 0x7F000001u;
    endpoint.port = port;
    return endpoint;
}

bool Endpoint::parse(const std::string& host, std::uint16_t port,
                     Endpoint& out) {
    in_addr address{};
    if (::inet_pton(AF_INET, host.c_str(), &address) != 1) {
        return false;
    }
    out.address = ntohl(address.s_addr);
    out.port = port;
    return true;
}

std::string Endpoint::toString() const {
    return std::to_string((address >> 24) & 0xFFu) + "." +
           std::to_string((address >> 16) & 0xFFu) + "." +
           std::to_string((address >> 8) & 0xFFu) + "." +
           std::to_string(address & 0xFFu) + ":" + std::to_string(port);
}

NetworkScope::NetworkScope() {
#if defined(_WIN32)
    WSADATA data{};
    ok_ = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    ok_ = true;
#endif
}

NetworkScope::~NetworkScope() {
#if defined(_WIN32)
    if (ok_) {
        ::WSACleanup();
    }
#endif
}

UdpSocket::~UdpSocket() { close(); }

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : handle_(other.handle_) {
    other.handle_ = kInvalidHandle;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = kInvalidHandle;
    }
    return *this;
}

bool UdpSocket::open() { return open(0); }

bool UdpSocket::open(std::uint16_t port) {
    close();

    const SocketHandle handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == kInvalidSocket) {
        return false;
    }

    const sockaddr_in address = toNative(Endpoint::loopback(port));
    sockaddr_in bindAddress = address;
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(handle, reinterpret_cast<const sockaddr*>(&bindAddress),
               static_cast<AddressLength>(sizeof(bindAddress))) != 0) {
        closeHandle(handle);
        return false;
    }

#if defined(_WIN32)
    u_long nonBlocking = 1;
    if (::ioctlsocket(handle, FIONBIO, &nonBlocking) != 0) {
        closeHandle(handle);
        return false;
    }
#else
    const int flags = ::fcntl(handle, F_GETFL, 0);
    if (flags < 0 || ::fcntl(handle, F_SETFL, flags | O_NONBLOCK) != 0) {
        closeHandle(handle);
        return false;
    }
#endif

    handle_ = static_cast<std::intptr_t>(handle);
    return true;
}

void UdpSocket::close() {
    if (handle_ == kInvalidHandle) {
        return;
    }
    closeHandle(static_cast<SocketHandle>(handle_));
    handle_ = kInvalidHandle;
}

std::uint16_t UdpSocket::localPort() const {
    if (!valid()) {
        return 0;
    }
    sockaddr_in address{};
    AddressLength length = static_cast<AddressLength>(sizeof(address));
    if (::getsockname(static_cast<SocketHandle>(handle_),
                      reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }
    return ntohs(address.sin_port);
}

bool UdpSocket::sendTo(const Endpoint& to, const void* data, std::size_t size) {
    if (!valid() || size > static_cast<std::size_t>(INT_MAX)) {
        return false;
    }
    const sockaddr_in address = toNative(to);
    const int sent = ::sendto(
        static_cast<SocketHandle>(handle_), reinterpret_cast<const char*>(data),
        static_cast<int>(size), 0, reinterpret_cast<const sockaddr*>(&address),
        static_cast<AddressLength>(sizeof(address)));
    return sent == static_cast<int>(size);
}

std::int32_t UdpSocket::receiveFrom(void* buffer, std::size_t capacity,
                                    Endpoint& from) {
    if (!valid() || capacity == 0) {
        return -1;
    }
    sockaddr_in address{};
    AddressLength length = static_cast<AddressLength>(sizeof(address));
    const int received = ::recvfrom(
        static_cast<SocketHandle>(handle_), reinterpret_cast<char*>(buffer),
        static_cast<int>(std::min<std::size_t>(capacity, INT_MAX)), 0,
        reinterpret_cast<sockaddr*>(&address), &length);
    if (received < 0) {
        return -1;
    }
    from.address = ntohl(address.sin_addr.s_addr);
    from.port = ntohs(address.sin_port);
    return received;
}

}  // namespace coopnet