#ifndef SDL1_CHUNK_H
#define SDL1_CHUNK_H
#include <map>

#include "Vector.h"

template<int N>
class Chunk {
public:
    size_t blocksNumber = N;
    Vector atPosition = Vector(0, 0, 0);
    // bool blocks[N][N][N]{};
    std::map<Vector, bool> blocks;


    Chunk() = default;

    Chunk(Vector position)
        : atPosition(position) {
    };
};

#endif //SDL1_CHUNK_H
