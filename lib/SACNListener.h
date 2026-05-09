#pragma once
#include "UdpSocket.h"

class SACNListener {
    public:

        SACNListener(uint16_t universe);
        ~SACNListener();

    private:

        void onPacket(const uint8_t* data, size_t size, const sockaddr_in* sender);

};