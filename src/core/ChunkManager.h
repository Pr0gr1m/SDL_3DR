#ifndef SDL1_CHUNKMANAGER_H
#define SDL1_CHUNKMANAGER_H
#include <vector>

#include "Chunk.h"

/**
 *Class storing all world chunks
*/

class ChunkManager {
public:
    ///Size of chunk in all axis
    static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 the chunk it goes from -8,-8,-8 and to 8,8,8

    ///Smallest number where logarythm of chunkSizeXYZ with the number as the base is a whole number
    static constexpr int smallestChunkSizeLogNumber = [] {
        int result = 1;
        for (int num = ChunkManager::chunkSizeXYZ; num != 1; --num) {
            int power = num;
            while (power < ChunkManager::chunkSizeXYZ) {
                power *= num;
            }
            if (power == ChunkManager::chunkSizeXYZ) {
                result = num;
                break;
            }
        }
        return result;
    }();

    std::vector<Chunk<chunkSizeXYZ> > worldChunks;
};

#endif //SDL1_CHUNKMANAGER_H
