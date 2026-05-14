#include "DMXUniverse.h"
#include <chrono>
#include <cstring>


double now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}


DMXUniverse::DMXUniverse() {
    this->universeNumber = 0;
    for (int i = 0; i < UNIVERSE_BUFFER_SIZE; i++) {
        this->data[i] = 0;
    }
    this->timestamp = now();
}

uint8_t DMXUniverse::getChannelValue(int channel) {
    if (channel >= UNIVERSE_BUFFER_SIZE) {
        return 0;
    }
    if (this->timestamp + UNIVERSE_EXPIRED_MS < now()) {
        return 0;
    }
    
    this->dataMutex[channel].lock();
    uint8_t data = this->data[channel];
    this->dataMutex[channel].unlock();
    return data;
};

void DMXUniverse::setChannelValue(int channel, uint8_t value) {
    if (channel >= UNIVERSE_BUFFER_SIZE) { 
        return;
    }
    this->dataMutex[channel].lock();
    this->data[channel] = value;
    this->dataMutex[channel].unlock();
    this->timestamp = now();
};

void DMXUniverse::clear() {
    for (int i = 0; i < UNIVERSE_BUFFER_SIZE; i++) {
        this->dataMutex[i].lock();
        this->data[i] = 0;
        this->dataMutex[i].unlock();
    }
};

void DMXUniverse::mergeInHTP(DMXUniverse* u1, DMXUniverse* u2) {
    for (int i = 0; i < UNIVERSE_BUFFER_SIZE; i++) {
        uint8_t v1 = u1->getChannelValue(i);
        uint8_t v2 = u2->getChannelValue(i);

        this->dataMutex[i].lock();
        if (v1 > v2) {
            this->data[i] = v1;
        } else {
            this->data[i] = v2;
        }
        this->dataMutex[i].unlock();
    }
    this->timestamp = now();
}

DMXUniverse::SACNPacket DMXUniverse::toSACNPacket() {

    const size_t headerSize = 126;
    const size_t dmxCount = UNIVERSE_BUFFER_SIZE;
    const size_t packetSize = headerSize + dmxCount;

    uint8_t* packetData = new uint8_t[packetSize]();

    // --- Root Layer ---
    packetData[0] = 0x00; // Preamble Size high
    packetData[1] = 0x10; // Preamble Size low
    packetData[2] = 0x00; // Post-amble Size high
    packetData[3] = 0x00; // Post-amble Size low

    // ACN Packet Identifier (12 bytes at offset 4)
    const uint8_t acn_id[12] = { 0x41, 0x53, 0x43, 0x2D, 0x45, 0x31, 0x2E, 0x31, 0x37, 0x00, 0x00, 0x00 };
    memcpy(packetData + 4, acn_id, 12);

    // Root Flags & Length at 16-17 (set below)

    // Root Vector = 0x00000004 at offset 18
    packetData[18] = 0x00;
    packetData[19] = 0x00;
    packetData[20] = 0x00;
    packetData[21] = 0x04;

    // CID (16 bytes at offset 22) - zeros is fine for now

    // --- Framing Layer ---
    // Framing Flags & Length at 38-39 (set below)

    // Framing Vector = 0x00000002 at offset 40
    packetData[40] = 0x00;
    packetData[41] = 0x00;
    packetData[42] = 0x00;
    packetData[43] = 0x02;

    // Source Name (64 bytes at offset 44)
    const char* sourceName = "Hayden Steele DMX Translator";
    strncpy(reinterpret_cast<char*>(packetData + 44), sourceName, 64);

    // Priority at 108
    packetData[108] = SACN_PRIORITY;

    // Synchronization Address at 109-110
    packetData[109] = 0x00;
    packetData[110] = 0x00;

    // Sequence Number at 111
    static uint8_t sequenceNumber = 0;
    packetData[111] = sequenceNumber++;

    // Options at 112
    packetData[112] = 0x00;

    // Universe at 113-114 (big-endian)
    packetData[113] = static_cast<uint8_t>((this->universeNumber >> 8) & 0xFF);
    packetData[114] = static_cast<uint8_t>(this->universeNumber & 0xFF);

    // --- DMP Layer ---
    // DMP Flags & Length at 115-116 (set below)

    // DMP Vector at 117
    packetData[117] = 0x02;

    // Address Type & Data Type at 118
    packetData[118] = 0xA1;

    // First Property Address at 119-120
    packetData[119] = 0x00;
    packetData[120] = 0x00;

    // Address Increment at 121-122
    packetData[121] = 0x00;
    packetData[122] = 0x01;

    // Property Value Count at 123-124 (512 channels + 1 start code)
    uint16_t propertyValueCount = static_cast<uint16_t>(dmxCount + 1);
    packetData[123] = static_cast<uint8_t>((propertyValueCount >> 8) & 0xFF);
    packetData[124] = static_cast<uint8_t>(propertyValueCount & 0xFF);

    // Start Code at 125
    packetData[125] = 0x00;

    // DMX Data at 126+
    for (size_t i = 0; i < dmxCount; ++i) {
        packetData[126 + i] = this->getChannelValue(static_cast<int>(i));
    }

    // --- Flags & Length fields ---
    // High nibble must be 0x7, lower 12 bits are the length
    auto setFlagsAndLength = [&](size_t offset, uint16_t length) {
        packetData[offset]     = static_cast<uint8_t>(0x70 | ((length >> 8) & 0x0F));
        packetData[offset + 1] = static_cast<uint8_t>(length & 0xFF);
    };

    setFlagsAndLength(16,  static_cast<uint16_t>(packetSize - 16));
    setFlagsAndLength(38,  static_cast<uint16_t>(packetSize - 38));
    setFlagsAndLength(115, static_cast<uint16_t>(packetSize - 115));

    DMXUniverse::SACNPacket packet;
    packet.data = packetData;
    packet.size = packetSize;
    packet.universe = this->universeNumber;
    return packet;
}