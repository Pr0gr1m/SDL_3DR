#ifndef SDL1_CHUNKMANAGER_H
#define SDL1_CHUNKMANAGER_H
#include <vector>

#include "Chunk.h"

class ChunkManager {
public:
    static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 the chunk it goes from -8,-8,-8 and to 8,8,8

    std::vector<Chunk<chunkSizeXYZ> > worldChunks;

    Object *GetObjectAt(Chunk<chunkSizeXYZ>, Vector);
};

#endif //SDL1_CHUNKMANAGER_H
