#include "UdpSocket.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <utility>

#ifdef _WIN32
static void initSockets() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
}

static void cleanupSockets() {
    WSACleanup();
}

static int closeSocket(SOCKET socketHandle) {
    return closesocket(socketHandle);
}
#else
static void initSockets() {}

static void cleanupSockets() {}

static int closeSocket(int socketHandle) {
    return close(socketHandle);
}
#endif

UdpSocket::UdpSocket(uint16_t port, Callback callback, const std::string &iface, bool useMulticast) {
    this->m_callback = std::move(callback);
    this->m_iface = iface;
    this->m_useMulticast = useMulticast;

    initSockets();

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
        throw std::runtime_error("socket() failed");

#ifdef _WIN32
    BOOL reuse = TRUE;
#else
    int reuse = 1;
#endif
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#if defined(__APPLE__)
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#endif

    // For receive sockets (callback provided) bind to requested interface/port.
    // For send-only sockets (no callback) bind to INADDR_ANY and ephemeral port.
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = iface.empty() ? INADDR_ANY : inet_addr(iface.c_str());

    if (::bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR)
        throw std::runtime_error("bind() failed");

    std::cout << "Listening on port: " << port << std::endl;

    m_thread = std::thread(&UdpSocket::listenLoop, this);

    // At the end of the UdpSocket constructor, after bind():
    if (!iface.empty()) {
        in_addr iface_addr{};
        iface_addr.s_addr = inet_addr(iface.c_str());
        setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_IF,
                reinterpret_cast<const char*>(&iface_addr), sizeof(iface_addr));

        unsigned char ttl = 4;
        setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL,
                reinterpret_cast<const char*>(&ttl), sizeof(ttl));
    }
}

UdpSocket::~UdpSocket() {
    m_running = false;
    closeSocket(m_socket);
    m_thread.join();
    cleanupSockets();
}

bool UdpSocket::joinMulticast(const std::string &groupAddr, const std::string &iface) {
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(groupAddr.c_str());
    mreq.imr_interface.s_addr = iface.empty() ? INADDR_ANY : inet_addr(iface.c_str());

    return setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char *>(&mreq), sizeof(mreq)) == 0;
}

void UdpSocket::listenLoop() {
    constexpr size_t BUF_SIZE = 65507;
    std::vector<uint8_t> buf(BUF_SIZE);

    while (m_running.load()) {
        sockaddr_in sender{};
    #ifdef _WIN32
        int senderLen = sizeof(sender);
    #else
        socklen_t senderLen = sizeof(sender);
    #endif

        int received = recvfrom(m_socket, reinterpret_cast<char *>(buf.data()), static_cast<int>(BUF_SIZE), 0, reinterpret_cast<sockaddr *>(&sender), &senderLen);

        if (received > 0 && m_callback)
            m_callback(buf.data(), static_cast<size_t>(received), sender);
        else if (received == SOCKET_ERROR)
            break;
    }
}

bool UdpSocket::send(const uint8_t* data, size_t dataSize, const std::string& address, uint16_t port) {
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = inet_addr(address.c_str());

    int sent = sendto(m_socket, reinterpret_cast<const char*>(data), static_cast<int>(dataSize), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));

    return sent != SOCKET_ERROR;
}