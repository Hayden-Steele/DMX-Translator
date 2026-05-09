#include "UdpSocket.h"
#include <stdexcept>
#include <vector>
#include <iostream>

UdpSocket::UdpSocket(uint16_t port, Callback callback, const std::string &iface, bool useMulticast) {
    this->m_callback = std::move(callback);
    
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw std::runtime_error("WSAStartup failed");

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
        throw std::runtime_error("socket() failed");

    BOOL reuse = TRUE;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = iface.empty() ? INADDR_ANY : inet_addr(iface.c_str());

    if (::bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR)
        throw std::runtime_error("bind() failed");

    std::cout << "Listening on port: " << port << std::endl;

    m_thread = std::thread(&UdpSocket::listenLoop, this);
}

UdpSocket::~UdpSocket() {
    m_running = false;
    closesocket(m_socket);
    m_thread.join();
    WSACleanup();
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
        int senderLen = sizeof(sender);

        int received = recvfrom(m_socket, reinterpret_cast<char *>(buf.data()), static_cast<int>(BUF_SIZE), 0, reinterpret_cast<sockaddr *>(&sender), &senderLen);

        if (received > 0 && m_callback)
            m_callback(buf.data(), static_cast<size_t>(received), sender);
        else if (received == SOCKET_ERROR)
            break;
    }
}