#include "UniverseStorage.h"


UniverseStorage::UniverseStorage(int count, int universeStart) {
    this->universeCount = count;
    this->universeStart = universeStart;
    this->universes = new DMXUniverse[count];
    for (int i = 0; i < count; i++) {
        this->universes[i].setUniverseNumber(universeStart + i);
    }
}

UniverseStorage::~UniverseStorage() {
    delete[] universes;
}

DMXUniverse* UniverseStorage::getUniverse(int index) {
    if (index < 0 || index >= universeCount) {
        return nullptr;
    }
    
    return &universes[index];
}

void UniverseStorage::mergeInHTP(UniverseStorage* s1, UniverseStorage* s2) {
    for (int i = 0; i < universeCount; i++) {
        this->universes[i].mergeInHTP(s1->getUniverse(i), s2->getUniverse(i));
    }
}

DMXUniverse::SACNPacket* UniverseStorage::toSACNPackets() {
    DMXUniverse::SACNPacket* packets = new DMXUniverse::SACNPacket[universeCount];
    for (int i = 0; i < universeCount; i++) {
        packets[i] = universes[i].toSACNPacket();
        packets[i].universe = universeStart + i;
    }
    return packets;
}