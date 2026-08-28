#ifndef SDL1_CHUNK_H
#define SDL1_CHUNK_H
#include <map>

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
    std::map<Vector, Object> blocks;

    Chunk() = default;

    Chunk(Vector position)
        : atPosition(position) {
    };
};

#endif //SDL1_CHUNK_H
