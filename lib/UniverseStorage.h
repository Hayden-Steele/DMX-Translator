#include "DMXUniverse.h"

class UniverseStorage { 

    public:
        UniverseStorage() : UniverseStorage(0, 0) {}
        UniverseStorage(int count, int universeStart);
        ~UniverseStorage();
        
        DMXUniverse* getUniverse(int index);
        int getUniverseCount() { return universeCount; }
        int getUniverseStart() { return universeStart; }

        void mergeInHTP(UniverseStorage* s1, UniverseStorage* s2);

        DMXUniverse::SACNPacket* toSACNPackets();

    private:
        int universeCount;
        int universeStart;
        DMXUniverse* universes;

};