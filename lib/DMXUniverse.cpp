#include "DMXUniverse.h"


uint8_t DMXUniverse::getChannelValue(int channel) {
    if (channel >= UNIVERSE_BUFFER_SIZE) {
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
}
