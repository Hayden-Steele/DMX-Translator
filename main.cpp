#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include "thread"
#include <cmath>
#include "lib/LightingParser.h"
#include "lib/configReader.h"

std::map<std::string, std::string> config = readConfig("config.txt");



#define UNIVERSE_COUNT getConfigInt(config, "universe_count", 2)
#define ARTNET_IN_UNIVERSE_START getConfigInt(config, "artnet_in_universe_start", 1)
#define SACN_IN_UNIVERSE_START getConfigInt(config, "sacn_in_universe_start", 5)
#define SACN_OUT_UNIVERSE_START getConfigInt(config, "sacn_out_universe_start", 1)
#define SACN_SEND_FPS getConfigInt(config, "sacn_send_fps", 40)


#define SACN_PORT 5568
#define ARTNET_PORT 6454
#define AVG_FRAME_TIME_SAMPLES 40


const double targetFrameTime = (1000.0 / SACN_SEND_FPS);
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


void statsLoop() {

    std::cout << "------------------------------------------------" << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "                                             " << std::endl;
        std::cout << "                                             " << std::endl;
        std::cout << "\033[2A";

        double avgFrameTime = 0;
        for (int i = 0; i < AVG_FRAME_TIME_SAMPLES; i++) {
            avgFrameTime += frameTimes[i];
        }
        avgFrameTime /= AVG_FRAME_TIME_SAMPLES;
        std::cout << "Avg Frame Send Time: ";
        if (avgFrameTime > targetFrameTime) {
            std::cout << "\033[31m";
        } else {
            std::cout << "\033[32m";
        }
        std::cout << std::floor(avgFrameTime * 100) / 100.0;
        std::cout << "\033[0m ms / " << targetFrameTime << " ms" << std::endl;

        double artnetAge = std::floor((now() - lastArtnet) * 10) / 10.0;
        double sacnAge = std::floor((now() - lastSACN) * 10) / 10.0;

        std::cout << "Artnet Age: ";
        if (artnetAge > UNIVERSE_EXPIRED_MS) {
            std::cout << "\033[31m";
        } else {
            std::cout << "\033[32m";
        }
        std::cout << artnetAge;
        std::cout << "\033[0m ms     SACN Age: ";
        if (sacnAge > UNIVERSE_EXPIRED_MS) {
            std::cout << "\033[31m";
        } else {
            std::cout << "\033[32m";
        }
        std::cout << sacnAge;
        std::cout << "\033[0m ms" << std::endl;

        std::cout << "\033[2A";
    }
}

bool rangesOverlap(int start1, int count1, int start2, int count2) {
    return (start1 < start2 + count2) && (start2 < start1 + count1);
}

int main() {

    std::cout << "Config: " << std::endl;   
    for (const auto& [key, value] : config) {
        std::cout << "  " << key << " = " << value << std::endl;
    }

    if (config["bind_address"].empty()) {
        throw std::runtime_error("Please provide a bind address in the config file (bind_address=)");
    } else {
        std::cout << "Using bind address: " << config["bind_address"] << std::endl;
    }

    if (rangesOverlap(SACN_OUT_UNIVERSE_START, UNIVERSE_COUNT, SACN_IN_UNIVERSE_START, UNIVERSE_COUNT)) {
        throw std::runtime_error("Input and output SACN universe ranges cannot overlap");
    }

    std::string bindAddress = config["bind_address"];

    LightingParser artnet(ARTNET_IN_UNIVERSE_START, UNIVERSE_COUNT, handleArtNetPacket, ARTNET_PORT, false, bindAddress);
    LightingParser sacn(SACN_IN_UNIVERSE_START, UNIVERSE_COUNT, handleSACNPacket, SACN_PORT, true, bindAddress);

    UdpSocket sendSocket(0, nullptr, bindAddress, false);
    
    std::thread statsThread(statsLoop);

    double start = now();

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

        double dt = now() - start;

        if (dt < targetFrameTime) {
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(targetFrameTime - dt));
        }

        frameCount++;
        frameTimes[frameCount % AVG_FRAME_TIME_SAMPLES] = dt;
    }

    return 0;
}