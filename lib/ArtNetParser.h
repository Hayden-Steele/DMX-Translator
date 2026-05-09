#pragma once
#include <string>
#include "UdpSocket.h"
#include "DMXUniverse.h"


#define ARTNET_PORT 6454

class ArtNetParser {

    public: 

        ArtNetParser(uint16_t universeStart, uint16_t universeCount, const std::string& iface = "");
        ~ArtNetParser();

        void handlePacket(const uint8_t* data, size_t dataSize);
        DMXUniverse* getUniverse(int universeIndex);

    private:
        int universeStart;
        int universeCount;

        UdpSocket* socket;
        DMXUniverse* universes;

};