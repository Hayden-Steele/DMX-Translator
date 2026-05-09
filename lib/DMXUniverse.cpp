#include "DMXUniverse.h"


inline uint8_t DMXUniverse::getChannelValue(int channel) {
    if (channel <= 0 || channel > UNIVERSE_BUFFER_SIZE) {
        return 0;
    }
    return data[channel - 1];
};

void DMXUniverse::mergeInHTP(DMXUniverse* u1, DMXUniverse* u2) {
    for (int i = 0; i < UNIVERSE_BUFFER_SIZE; i++) {
        uint8_t v1 = u1->data[i];
        uint8_t v2 = u2->data[i];

        if (v1 > v2) {
            this->data[i] = v1;
        } else {
            this->data[i] = v2;
        }
    }
}
