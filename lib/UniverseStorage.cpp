#include "UniverseStorage.h"


UniverseStorage::UniverseStorage(int count) {
    this->universeCount = count;
    this->universes = new DMXUniverse[count];
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