#ifndef SDL1_CHUNK_H
#define SDL1_CHUNK_H
#include <array>
#include <optional>

#include "GlobalVariables.h"
#include "Object.h"
#include "Vector.h"

/**
 *@class Chunk
 *@brief Class representing 1 chunk, storing its blocks
**/
template<int N>
class Chunk {
public:
    ///Chunk global offset
    Vector atPosition = Vector(0, 0, 0);
    ///Map of all objects with local offset to chunk root
    std::array<std::optional<Object>, N * N * N> blocks; //ai says it can be faster (10x-100x)
    ///True when blocks changed since the mesh was last built
    bool meshDirty = true;

    Chunk() = default;

    Chunk(Vector position)
        : atPosition(position) {
    };

    [[nodiscard]] static constexpr bool isPosInBounds(int x, int y, int z) {
        return x >= 0 && x < N && y >= 0 && y < N && z >= 0 && z < N;
    }

    [[nodiscard]] static constexpr int chunkIndex(int x, int y, int z) {
        return (y * N + z) * N + x;
    }

    void tryInsert(const Vector &p, Object object) {
        const int x = toInt(p.x), y = toInt(p.y), z = toInt(p.z);
        if (!isPosInBounds(x, y, z)) return;
        blocks[chunkIndex(x, y, z)] = object;
        meshDirty = true;
    }

    ///Removes the block at relative position in the chunk, returns whether a block was removed
    bool tryRemove(const Vector &p) {
        const int x = toInt(p.x), y = toInt(p.y), z = toInt(p.z);
        if (!isPosInBounds(x, y, z)) return false;
        auto &block = blocks[chunkIndex(x, y, z)];
        if (!block.has_value()) return false;
        block.reset();
        meshDirty = true;
        return true;
    }

    ///Returns optional Object at relative position in the chunk
    [[nodiscard]] const std::optional<Object> *tryGet(const Vector &p) const {
        const int x = toInt(p.x), y = toInt(p.y), z = toInt(p.z);
        if (!isPosInBounds(x, y, z)) return nullptr;
        return &blocks[chunkIndex(x, y, z)];
    }

    [[nodiscard]] bool PositionInBounds(Vector vector) const {
        return vector.x >= atPosition.x && vector.x < atPosition.x + N && vector.y >= atPosition.y && vector.y < atPosition.y + N && vector.z >= atPosition.z && vector.z < atPosition.z + N;
    };
};

#endif //SDL1_CHUNK_H
