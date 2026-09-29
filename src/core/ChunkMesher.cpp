#include "ChunkMesher.h"

#include <array>

#include "Face.h"

namespace {
    ///Neighbor cell offset per face direction, in the same order as the faces: +Y, -Y, +X, -X, +Z, -Z
    constexpr std::array<std::array<int, 3>, 6> kNeighborOffsets = {
        {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}
    };

    ///Indices into the block's eight corners of the two triangles of each face direction
    constexpr std::array<std::array<std::array<int, 3>, 2>, 6> kFaceCorners = {
        {
            {{{4, 5, 6}, {4, 6, 7}}}, // top, +Y
            {{{0, 2, 1}, {0, 3, 2}}}, // bottom, -Y
            {{{3, 7, 6}, {3, 6, 2}}}, // right, +X
            {{{0, 1, 5}, {0, 5, 4}}}, // left, -X
            {{{1, 2, 6}, {1, 6, 5}}}, // front, +Z
            {{{0, 4, 7}, {0, 7, 3}}} // back, -Z
        }
    };
}

std::vector<Vertex3D> buildChunkMesh(const Chunk<ChunkManager::chunkSizeXYZ> &chunk, const ChunkManager &chunkManager) {
    std::vector<Vertex3D> vertices;

    constexpr int size = ChunkManager::chunkSizeXYZ;
    for (int y = 0; y < size; y += 1) {
        for (int z = 0; z < size; z += 1) {
            for (int x = 0; x < size; x += 1) {
                if (!chunk.blocks[chunk.chunkIndex(x, y, z)].has_value()) continue;

                const float minX = static_cast<float>(x) - kBlockHalfExtent;
                const float minY = static_cast<float>(y) - kBlockHalfExtent;
                const float minZ = static_cast<float>(z) - kBlockHalfExtent;
                const float maxX = static_cast<float>(x) + kBlockHalfExtent;
                const float maxY = static_cast<float>(y) + kBlockHalfExtent;
                const float maxZ = static_cast<float>(z) + kBlockHalfExtent;

                const std::array<Vector, 8> corners = {
                    Vector(minX, minY, minZ), Vector(minX, minY, maxZ), Vector(maxX, minY, maxZ), Vector(maxX, minY, minZ),
                    Vector(minX, maxY, minZ), Vector(minX, maxY, maxZ), Vector(maxX, maxY, maxZ), Vector(maxX, maxY, minZ)
                };

                //Faces are built in world space so UVs match world positions, then rebased to chunk-local positions
                for (size_t direction = 0; direction < kFaceCorners.size(); direction += 1) {
                    const auto &offset = kNeighborOffsets[direction];
                    if (chunkManager.isNeighborCellSolid(chunk, x + offset[0], y + offset[1], z + offset[2])) continue;

                    for (const auto &triangle: kFaceCorners[direction]) {
                        Face face(corners[triangle[0]], corners[triangle[1]], corners[triangle[2]]);
                        face += chunk.atPosition;
                        for (auto vertex: face.GetFaceDrawCallVerticies()) {
                            vertex.Position = vertex.Position - chunk.atPosition;
                            vertices.push_back(vertex);
                        }
                    }
                }
            }
        }
    }

    return vertices;
}
