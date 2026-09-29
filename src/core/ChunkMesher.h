#ifndef SDL1_CHUNKMESHER_H
#define SDL1_CHUNKMESHER_H
#include <vector>

#include "ChunkManager.h"
#include "Vertex3D.h"

///Builds the chunk-local triangle list (12 triangles per block) of the chunk, empty for an empty chunk
[[nodiscard]] std::vector<Vertex3D> buildChunkMesh(const Chunk<ChunkManager::chunkSizeXYZ> &chunk, const ChunkManager &chunkManager);

#endif //SDL1_CHUNKMESHER_H
