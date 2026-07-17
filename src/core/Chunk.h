#ifndef SDL1_CHUNK_H
#define SDL1_CHUNK_H
#include <map>

#include "Object.h"
#include "Vector.h"

template<int N>
class Chunk {
public:
    Vector atPosition = Vector(0, 0, 0);
    std::map<Vector, Object> blocks;

    Chunk() = default;

    Chunk(Vector position)
        : atPosition(position) {
    };
};

#endif //SDL1_CHUNK_H
