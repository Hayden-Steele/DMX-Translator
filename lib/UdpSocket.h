#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

class UdpSocket {
    public:
        using Callback = std::function<void(const uint8_t*, size_t, const sockaddr_in&)>;

        UdpSocket(uint16_t port, Callback callback, const std::string& iface = "");
        ~UdpSocket();

        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;

        bool joinMulticast(const std::string& groupAddr, const std::string& iface = "");

    private:
        void listenLoop();

        SOCKET            m_socket = INVALID_SOCKET;
        std::thread       m_thread;
        std::atomic<bool> m_running { true };
        Callback          m_callback;
};