#pragma once
#include <cstdint>
#include <string>
#include <iostream>

#define UNIVERSE_BUFFER_SIZE 256

class DMXUniverse {
    public: 

        inline uint8_t getChannelValue(int channel);
        void mergeInHTP(DMXUniverse* u1, DMXUniverse* u2);

        friend std::ostream& operator<<(std::ostream& os, const DMXUniverse& u) {
            for (int i = 0; i < UNIVERSE_BUFFER_SIZE; i++) {
                os << static_cast<int>(u.data[i]) << " ";
            }
            return os;
        };

    private:
        uint8_t *data = new uint8_t[UNIVERSE_BUFFER_SIZE];
};