#include "LightingParser.h"





std::string calcMulticastIp(uint16_t universe) {
    return "239.255." + std::to_string(universe >> 8) + "." + std::to_string(universe & 0xFF);
}


LightingParser::LightingParser(uint16_t universeStart, uint16_t universeCount, PacketHandler packetHandler, uint16_t port, bool useMulticast, const std::string& iface) {
    this->universeStart = universeStart;
    this->universeCount = universeCount;
    this->packetHandler = packetHandler;
    this->universeStorage = new UniverseStorage(universeCount, universeStart);

    this->socket = new UdpSocket(port, [this](const uint8_t* data, size_t dataSize, const sockaddr_in& sender) {
        this->packetHandler(this, data, dataSize);
    }, iface, useMulticast);

    if (useMulticast) {
        // Join multicast group for SACN
        for (int i = 0; i < universeCount; i++) {
            this->socket->joinMulticast(calcMulticastIp(universeStart + i), iface);
        }
    }
}

LightingParser::~LightingParser() {
    delete socket;
}