#ifndef SDL1_CHUNKCACHE_H
#define SDL1_CHUNKCACHE_H
#include "Face.h"

/**
 * @struct LODDBLock
 * @brief Represents a block after LOD
**/
struct LODDBLock {
    std::array<Face, 12> faces{};
    std::byte numFaces{};

    LODDBLock() = default;

    LODDBLock(std::array<Face, 12> faces) : faces(faces) {
    }
};

/**
* @struct ChunkMeshCache
* @brief Represents cached chunk mesh made up of LODDBlocks.
 **/
template<int N>
struct ChunkMeshCache {
    std::vector<LODDBLock> lodBlocks;
    int cachedLOD = -1;
    bool isValidCache = false;
    std::array<bool, N * N * N> cachedBlockPresence;
};
#endif //SDL1_CHUNKCACHE_H
