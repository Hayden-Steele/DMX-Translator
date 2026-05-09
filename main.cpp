#include <iostream>
#include <vector>
#include <cstring>
#include "lib/LightingParser.h"



#define UNIVERSE_COUNT 1

#define ARTNET_IN_UNIVERSE_START 1
#define SACN_IN_UNIVERSE_START 2

#define SACN_OUT_UNIVERSE_START 3

#define SACN_PORT 5568
#define ARTNET_PORT 6454
#define SACN_SEND_FPS 45



void handleArtNetPacket(LightingParser* parser, const uint8_t* data, size_t dataSize) {
    // Size Check
    if (dataSize < 18) { return; }
    // Header check
    if (data[0] != 65 || data[1] != 114 || data[2] != 116 || data[3] != 45 || data[4] != 78 || data[5] != 101 || data[6] != 116) { return; }

    uint8_t subUniverse = data[14];
    uint8_t net = data[15];
    uint16_t universe = (net << 8) | subUniverse;

    uint16_t dmxLength = (data[16] << 8) | data[17];
    const uint8_t* dmxData = data + 18;

    int universeIndex = universe - parser->getUniverseStart();
    if (universeIndex < 0 || universeIndex >= parser->getUniverseCount()) {
        return;
    }

    for (int i = 0; i < dmxLength; i++) {
        parser->getUniverseStorage()->getUniverse(universeIndex)->setChannelValue(i, dmxData[i]);
    }

    std::cout << "ARTNET " << universe << " - " << *parser->getUniverseStorage()->getUniverse(universeIndex) << std::endl;
}


static constexpr uint8_t SACN_SIGNATURE[] = {
    0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e,
    0x31, 0x37, 0x00, 0x00, 0x00
};

void handleSACNPacket(LightingParser* parser, const uint8_t* data, size_t dataSize) {

    if (dataSize < 126) return;
    if (memcmp(data + 4, SACN_SIGNATURE, sizeof(SACN_SIGNATURE)) != 0) return;

    uint16_t universe = (data[113] << 8) | data[114];
    if (data[125] != 0x00) return;

    int universeIndex = universe - parser->getUniverseStart();
    if (universeIndex < 0 || universeIndex >= parser->getUniverseCount()) {
        return;
    }

    const uint8_t* dmxData = data + 126;
    size_t dmxLength = dataSize - 126;

    for (size_t i = 0; i < dmxLength; i++) {
        parser->getUniverseStorage()->getUniverse(universeIndex)->setChannelValue(i, dmxData[i]);
    }

    std::cout << "SACN " << universe << " - " << *parser->getUniverseStorage()->getUniverse(universeIndex) << std::endl;
}




UniverseStorage output = UniverseStorage(UNIVERSE_COUNT);


int main() {

    LightingParser artnet = LightingParser(ARTNET_IN_UNIVERSE_START, UNIVERSE_COUNT, handleArtNetPacket, ARTNET_PORT, false, "2.0.0.3");
    LightingParser sacn = LightingParser(SACN_IN_UNIVERSE_START, UNIVERSE_COUNT, handleSACNPacket, SACN_PORT, true, "2.0.0.3");


    std::cin.get();

    return 0;
}