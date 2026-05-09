#include "DMXUniverse.h"

class UniverseStorage { 

    public:
        UniverseStorage() : UniverseStorage(0) {}
        UniverseStorage(int count);
        ~UniverseStorage();
        
        DMXUniverse* getUniverse(int index);
        int getUniverseCount() { return universeCount; }

        void mergeInHTP(UniverseStorage* s1, UniverseStorage* s2);

    private:
        int universeCount;
        DMXUniverse* universes;

};