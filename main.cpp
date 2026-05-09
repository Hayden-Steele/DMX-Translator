#include <iostream>
#include <vector>
#include <cstring>
#include "lib/UdpSocket.h"

#define UNIVERSE_COUNT 1

#define ARTNET_IN_UNIVERSE_START 1
#define SACN_IN_UNIVERSE_START 2

#define SACN_OUT_UNIVERSE_START 3

#define ARTNET_PORT 6454
#define SACN_PORT 5568


void printData(const uint8_t *data, size_t dataSize, const sockaddr_in &sock) {
    for (int i = 0; i < dataSize; i++) {
        std::cout << data[i];
    }
    std::cout << std::endl;
}

int main() {

    UdpSocket art = UdpSocket(ARTNET_PORT, printData, "2.0.0.3");
    UdpSocket sacn = UdpSocket(SACN_PORT, printData, "2.0.0.3");

    while (true) {}

    return 0;
}