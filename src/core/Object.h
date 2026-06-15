#include "Vector.h"
#include "../Mesh.h"

#ifndef SDL1_OBJECT_H
#define SDL1_OBJECT_H

class Object {
public:
    Mesh Mesh;
    Vector Position;
    Vector Rotation;
};

#endif //SDL1_OBJECT_H

