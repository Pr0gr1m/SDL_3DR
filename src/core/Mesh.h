#ifndef SDL1_MESH_H
#define SDL1_MESH_H

#include "Vector.h"
#include "Int3.h"

#include "Face.h"

/**
 *@class Mesh
 *@brief A Trivial class representing mesh, containing verticies and triangles pointers
 */
class Mesh {
public:
    ~Mesh() = default;

    ///Pointer to verticies array
    Vector *verticies = nullptr;
    ///Pointer to triangles array
    Int3 *triangles = nullptr;

    ///Num of verticies in verticies array
    int numVerticies = 0;
    ///Num of triangles in triangles array
    int numTriangles = 0;

    Mesh() = default;

    Mesh(Vector *verticies, int numVerticies, Int3 *triangles, int numTriangles)
        : verticies(verticies), triangles(triangles), numVerticies(numVerticies), numTriangles(numTriangles) {
    }

    bool operator==(const Mesh &mesh) const {
        return numVerticies == mesh.numVerticies && verticies == mesh.verticies && numTriangles == mesh.numTriangles && triangles == mesh.triangles;
    }
};

#endif //SDL1_MESH_H
