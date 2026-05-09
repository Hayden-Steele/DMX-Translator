#include <iostream>
#include <vector>
#include <cstring>
#include "lib/UdpSocket.h"
#include "lib/DMXUniverse.h"
#include "lib/ArtNetParser.h"

#define UNIVERSE_COUNT 1

#define ARTNET_IN_UNIVERSE_START 1
#define SACN_IN_UNIVERSE_START 2

#define SACN_OUT_UNIVERSE_START 3

#define SACN_PORT 5568


void printData(const uint8_t *data, size_t dataSize, const sockaddr_in &sock) {

    // Size Check
    if (dataSize < 18) { return; }
    // Header check
    if (data[0] != 65 || data[1] != 114 || data[2] != 116 || data[3] != 45 || data[4] != 78 || data[5] != 101 || data[6] != 116) { return; }

    uint8_t subUniverse = data[14];
    uint8_t net = data[15];
    uint16_t universe = (net << 8) | subUniverse;

    uint16_t length = (data[16] << 8) | data[17];
    const uint8_t* dmxData = data + 18;

    std::cout << "RECIEVED UNIVERRSE " << universe << " - ";
    
    for (int i = 0; i < length; i++) {
        std::cout << +dmxData[i] << " ";
    }
    std::cout << std::endl;
}

int main() {

    // UdpSocket sacn = UdpSocket(SACN_PORT, printData, "2.0.0.3");

    ArtNetParser parser = ArtNetParser(ARTNET_IN_UNIVERSE_START, UNIVERSE_COUNT, "2.0.0.3");

    while (true) {}

    return 0;
}