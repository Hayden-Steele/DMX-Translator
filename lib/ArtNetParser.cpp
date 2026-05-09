#include "ArtNetParser.h"



ArtNetParser::ArtNetParser(uint16_t universeStart, uint16_t universeCount, const std::string& iface) {
    this->universeStart = universeStart;
    this->universeCount = universeCount;
    
    this->universes = new DMXUniverse[universeCount];
    
    for (int i = 0; i < universeCount; i++) {
        this->universes[i] = DMXUniverse();
    }

    this->socket = new UdpSocket(ARTNET_PORT, [this](const uint8_t* data, size_t dataSize, const sockaddr_in& sender) {
        this->handlePacket(data, dataSize);
    }, iface);
}

ArtNetParser::~ArtNetParser() {
    delete[] this->universes;
    delete this->socket;
}

void ArtNetParser::handlePacket(const uint8_t* data, size_t dataSize) {
    // Size Check
    if (dataSize < 18) { return; }
    // Header check
    if (data[0] != 65 || data[1] != 114 || data[2] != 116 || data[3] != 45 || data[4] != 78 || data[5] != 101 || data[6] != 116) { return; }

    uint8_t subUniverse = data[14];
    uint8_t net = data[15];
    uint16_t universe = (net << 8) | subUniverse;

    uint16_t length = (data[16] << 8) | data[17];
    const uint8_t* dmxData = data + 18;

    int universeIndex = universe - this->universeStart;
    if (universeIndex < 0 || universeIndex >= this->universeCount) {
        return;
    }

    for (int i = 0; i < length; i++) {
        this->universes[universeIndex].setChannelValue(i, dmxData[i]);
    }

    std::cout << "RECIEVED UNIVERRSE " << universe << " - " << this->universes[universeIndex] << std::endl;
}

DMXUniverse* ArtNetParser::getUniverse(int universeIndex) {
    if (universeIndex < 0 || universeIndex >= this->universeCount) {
        return nullptr;
    }
    return &this->universes[universeIndex];
}