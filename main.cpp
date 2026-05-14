#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include "thread"
#include "lib/LightingParser.h"



#define UNIVERSE_COUNT 100

#define ARTNET_IN_UNIVERSE_START 1
#define SACN_IN_UNIVERSE_START 1

#define SACN_OUT_UNIVERSE_START 200

#define SACN_PORT 5568
#define ARTNET_PORT 6454
#define SACN_SEND_FPS 30

#define BIND_ADDRESS "10.0.0.236"



#define AVG_FRAME_TIME_SAMPLES 90
double frameTimes[AVG_FRAME_TIME_SAMPLES] = {0};
uint64_t frameCount = 0;

double lastArtnet = 0;
double lastSACN = 0;

UniverseStorage output(UNIVERSE_COUNT, SACN_OUT_UNIVERSE_START);


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

    lastArtnet = now();
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

    lastSACN = now();
}


int main() {

    std::cout << "Binding to address: " << BIND_ADDRESS << std::endl;

    LightingParser artnet(ARTNET_IN_UNIVERSE_START, UNIVERSE_COUNT, handleArtNetPacket, ARTNET_PORT, false, BIND_ADDRESS);
    LightingParser sacn(SACN_IN_UNIVERSE_START, UNIVERSE_COUNT, handleSACNPacket, SACN_PORT, true, BIND_ADDRESS);

    UdpSocket sendSocket(0, nullptr, BIND_ADDRESS, false);
    
    double start = now();
    double end = now();

    const double targetFrameTime = (1000.0 / SACN_SEND_FPS);

    while (true) {
        start = now();

        output.mergeInHTP(artnet.getUniverseStorage(), sacn.getUniverseStorage());
        DMXUniverse::SACNPacket* packets = output.toSACNPackets();
        for (int i = 0; i < output.getUniverseCount(); i++) {
        
            bool success = sendSocket.send(packets[i].data, packets[i].size, calcMulticastIp(packets[i].universe), SACN_PORT);
            if (!success) {
                std::cerr << "Failed to send SACN packet for universe " << (SACN_OUT_UNIVERSE_START + i) << std::endl;
            }
            delete[] packets[i].data;
        }
        delete[] packets;
    

        end = now();
        double dt = end - start;

        if (dt < targetFrameTime) {
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(targetFrameTime - dt));
        }

        frameCount++;
        frameTimes[frameCount % AVG_FRAME_TIME_SAMPLES] = dt;
        if (frameCount % AVG_FRAME_TIME_SAMPLES == 0) {

            std::cout << "------------------------------------------------" << std::endl;

            double avgFrameTime = 0;
            for (int i = 0; i < AVG_FRAME_TIME_SAMPLES; i++) {
                avgFrameTime += frameTimes[i];
            }
            avgFrameTime /= AVG_FRAME_TIME_SAMPLES;
            std::cout << "Avg Sleep Time: " << avgFrameTime << " ms" << std::endl;

            double artnetAge = now() - lastArtnet;
            double sacnAge = now() - lastSACN;
            std::cout << "Artnet Age: " << artnetAge << " ms     SACN Age: " << sacnAge << " ms" << std::endl;
        }
    }

    std::cin.get();

    return 0;
}