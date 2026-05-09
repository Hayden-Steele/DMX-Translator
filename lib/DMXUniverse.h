#pragma once
#include <cstdint>
#include <string>
#include <iostream>

#define UNIVERSE_BUFFER_SIZE 512

class DMXUniverse {
    public: 

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

    private:
        uint8_t data[UNIVERSE_BUFFER_SIZE];
};