#ifndef SDL1_MESH_H
#define SDL1_MESH_H

#include "Vector.h"
#include "Int3.h"


class Mesh {
public:
    Vector *verticies = nullptr;
    Int3 *triangles = nullptr;

    int numVerticies = 0;
    int numTriangles = 0;

    Mesh() = default;

    Mesh(Vector *verticies, int numVerticies, Int3 *triangles, int numTriangles)
        : verticies(verticies), triangles(triangles), numVerticies(numVerticies), numTriangles(numTriangles) {
    }

    // void Set(Vector *verticies, int numVerticies, Int3 *triangles = nullptr, int numTriangles = 0) {
    //     this->verticies = verticies;
    //     this->triangles = triangles;
    //
    //     this->numVerticies = numVerticies;
    //     this->numTriangles = numTriangles;
    // }

    bool operator==(const Mesh &mesh) {
        return numVerticies == mesh.numVerticies && verticies == mesh.verticies;
    }
};

#endif //SDL1_MESH_H
