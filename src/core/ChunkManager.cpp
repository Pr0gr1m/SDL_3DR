#include "ChunkManager.h"

Object *ChunkManager::GetObjectAt(Chunk<chunkSizeXYZ> chunk, Vector relativeVectorOffset) {
    for (auto &block: chunk.blocks) {
        if (block.first == relativeVectorOffset) {
            return &block.second;
        }
    }

    return nullptr;
}
