#pragma once
#include <string>
#include "UdpSocket.h"
#include "DMXUniverse.h"
#include "UniverseStorage.h"

class LightingParser {

    public:

        using PacketHandler = std::function<void(LightingParser* parser, const uint8_t* data, size_t dataSize)>;

        LightingParser(uint16_t universeStart, uint16_t universeCount, PacketHandler packetHandler, uint16_t port, bool useMulticast = false, const std::string& iface = "");
        ~LightingParser();

        UniverseStorage* getUniverseStorage() { return universeStorage; }  
        int getUniverseStart() const { return universeStart; }
        int getUniverseCount() const { return universeCount; }

    private:

        int universeStart;
        int universeCount;

        PacketHandler packetHandler;
        UdpSocket* socket;
        UniverseStorage* universeStorage;

};

std::string calcMulticastIp(uint16_t universe);