#pragma once
#include <cstdint>
#include <string>
#include <iostream>
#include <mutex>

#define UNIVERSE_BUFFER_SIZE 512

class DMXUniverse {
    public: 

        DMXUniverse();

        void setUniverseNumber(uint16_t universeNumber) { this->universeNumber = universeNumber; }

        uint8_t getChannelValue(int channel);
        void setChannelValue(int channel, uint8_t value);

        void clear();

        void mergeInHTP(DMXUniverse* u1, DMXUniverse* u2);

        friend std::ostream& operator<<(std::ostream& os, const DMXUniverse& u) {
            for (int i = 0; i < 100; i++) {
                os << static_cast<int>(u.data[i]) << " ";
            }
            return os;
        };

        struct SACNPacket {
            uint8_t* data = nullptr;
            size_t size = 0;
            uint16_t universe = 0;
        };
        SACNPacket toSACNPacket();
        

    private:
        uint8_t data[UNIVERSE_BUFFER_SIZE];
        std::mutex dataMutex[UNIVERSE_BUFFER_SIZE];
        uint16_t universeNumber;
};