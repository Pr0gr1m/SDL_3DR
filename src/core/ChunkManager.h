#ifndef SDL1_CHUNKMANAGER_H
#define SDL1_CHUNKMANAGER_H
#include <vector>
#include <SDL3/SDL_log.h>

#include "Chunk.h"

static int smallestChunkSizeLogNumber;

inline double log(double base, double number) {
    return std::log(number) / std::log(base);
}

class ChunkManager {
public:
    static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 the chunk it goes from -8,-8,-8 and to 8,8,8

    ChunkManager() {
        for (int num = chunkSizeXYZ; num != 1; num--) {
            auto currentLogarythm = log(num, chunkSizeXYZ);
            if (ceil(currentLogarythm) == currentLogarythm) //whole number
                smallestChunkSizeLogNumber = num;
        }
    }

    std::vector<Chunk<chunkSizeXYZ> > worldChunks;

    Object *GetObjectAt(Chunk<chunkSizeXYZ>, Vector);
};

#endif //SDL1_CHUNKMANAGER_H
