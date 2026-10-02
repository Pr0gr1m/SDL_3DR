#ifndef SDL1_CHUNKMANAGER_H
#define SDL1_CHUNKMANAGER_H
#include <array>
#include <cmath>
#include <vector>
#include <map>
#include <unordered_map>
#include <utility>

#include "Chunk.h"
#include "Int3.h"

/**
 *Class storing all world chunks
*/

class ChunkManager {
public:
    ///Size of chunk in all axis
    static constexpr int chunkSizeXYZ = 16; //starting from 0,0,0 the chunk it goes from -8,-8,-8 and to 8,8,8

    ///All stored world chunks
    std::vector<Chunk<chunkSizeXYZ> > worldChunks;

    ///Map of index based on atPosition of the chunk
    std::map<Int3, size_t> chunkMap;

    ///Returns the atPosition of the chunk containing the block coordinate along one axis
    [[nodiscard]] static int chunkCoordinateFromBlockCoordinate(const int blockCoordinate) {
        return static_cast<int>(std::floor(static_cast<float>(blockCoordinate) / chunkSizeXYZ)) * chunkSizeXYZ;
    }

    ///Returns the chunkMap key of the chunk containing the world block position
    [[nodiscard]] static Int3 chunkLookupFromBlockPosition(const Vector &blockPosition) {
        return {
            chunkCoordinateFromBlockCoordinate(static_cast<int>(blockPosition.x)),
            chunkCoordinateFromBlockCoordinate(static_cast<int>(blockPosition.y)),
            chunkCoordinateFromBlockCoordinate(static_cast<int>(blockPosition.z))
        };
    }

    ///Returns the chunk stored at the given chunkMap key, or nullptr when it is not loaded
    [[nodiscard]] Chunk<chunkSizeXYZ> *findChunk(const Int3 &chunkLookup) {
        const auto iterator = chunkMap.find(chunkLookup);
        return iterator == chunkMap.end() ? nullptr : &worldChunks[iterator->second];
    }

    [[nodiscard]] const Chunk<chunkSizeXYZ> *findChunk(const Int3 &chunkLookup) const {
        const auto iterator = chunkMap.find(chunkLookup);
        return iterator == chunkMap.end() ? nullptr : &worldChunks[iterator->second];
    }

    ///Whether the cell at chunk-local coordinates (each in [-1, chunkSizeXYZ]) holds a block, reading loaded neighbor chunks across borders
    ///Cells in unloaded neighbors are solid on X and Z (no side faces towards nothing) and air on Y (terrain tops and bottoms stay visible)
    [[nodiscard]] bool isNeighborCellSolid(const Chunk<chunkSizeXYZ> &chunk, const int x, const int y, const int z) const {
        if (Chunk<chunkSizeXYZ>::isPosInBounds(x, y, z)) return chunk.blocks[Chunk<chunkSizeXYZ>::chunkIndex(x, y, z)].has_value();

        const auto neighborOffset = [](const int local) {
            if (local < 0) return -chunkSizeXYZ;
            return local >= chunkSizeXYZ ? chunkSizeXYZ : 0;
        };
        const Int3 chunkLookup = chunk.atPosition.toInt3();
        const Chunk<chunkSizeXYZ> *neighbor = findChunk({
            chunkLookup.a + neighborOffset(x),
            chunkLookup.b + neighborOffset(y),
            chunkLookup.c + neighborOffset(z)
        });
        if (neighbor == nullptr) return !Chunk<chunkSizeXYZ>::isPosInBounds(x, 0, z);

        const auto wrap = [](const int local) { return (local + chunkSizeXYZ) % chunkSizeXYZ; };
        return neighbor->blocks[Chunk<chunkSizeXYZ>::chunkIndex(wrap(x), wrap(y), wrap(z))].has_value();
    }

    ///Removes the block at the world position and marks the affected meshes dirty, never creates chunks
    bool removeBlock(const Vector &worldBlockPosition) {
        Chunk<chunkSizeXYZ> *chunk = findChunk(chunkLookupFromBlockPosition(worldBlockPosition));
        if (chunk == nullptr) return false;

        const Vector localPosition = worldBlockPosition - chunk->atPosition;
        if (!chunk->tryRemove(localPosition)) return false;

        markBoundaryNeighborsDirty(*chunk, localPosition);
        return true;
    }

    ///Places the block at the world position, creating its chunk when missing, and marks the affected meshes dirty
    void setBlock(const Vector &worldBlockPosition, const Object &object) {
        const Int3 chunkLookup = chunkLookupFromBlockPosition(worldBlockPosition);
        if (findChunk(chunkLookup) == nullptr) {
            worldChunks.emplace_back(chunkLookup.toVector());
            chunkMap.insert({chunkLookup, worldChunks.size() - 1});
            for (int axis = 0; axis < 3; axis += 1) {
                markNeighborDirty(chunkLookup, axis, -1);
                markNeighborDirty(chunkLookup, axis, 1);
            }
        }

        Chunk<chunkSizeXYZ> &chunk = *findChunk(chunkLookup);
        const Vector localPosition = worldBlockPosition - chunk.atPosition;
        chunk.tryInsert(localPosition, object);
        markBoundaryNeighborsDirty(chunk, localPosition);
    }

    // private:
    ///Marks the loaded chunk next to chunkLookup along the axis (0 = x, 1 = y, 2 = z) in the given direction (-1 or 1) dirty
    void markNeighborDirty(const Int3 &chunkLookup, const int axis, const int direction) {
        std::array<int, 3> neighborLookup{chunkLookup.a, chunkLookup.b, chunkLookup.c};
        neighborLookup[axis] += direction * chunkSizeXYZ;
        if (Chunk<chunkSizeXYZ> *neighbor = findChunk({neighborLookup[0], neighborLookup[1], neighborLookup[2]})) {
            neighbor->meshDirty = true;
        }
    }

    ///Marks face neighbors dirty when the block at the chunk-local position lies on a chunk border
    void markBoundaryNeighborsDirty(const Chunk<chunkSizeXYZ> &chunk, const Vector &localPosition) {
        const std::array<int, 3> local{toInt(localPosition.x), toInt(localPosition.y), toInt(localPosition.z)};
        const Int3 chunkLookup = chunk.atPosition.toInt3();
        for (int axis = 0; axis < 3; axis += 1) {
            if (local[axis] == 0) markNeighborDirty(chunkLookup, axis, -1);
            if (local[axis] == chunkSizeXYZ - 1) markNeighborDirty(chunkLookup, axis, 1);
        }
    }
};

#endif //SDL1_CHUNKMANAGER_H
