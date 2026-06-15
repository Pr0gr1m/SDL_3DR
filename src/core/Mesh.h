#ifndef SDL1_MESH_H
#define SDL1_MESH_H
#include "../Int2.h"
#include "Vector.h"

class Mesh {
public:
    Vector *verticies;
    Int2 *triagnles;
};

#endif //SDL1_MESH_H
