#ifndef SDL1_MESH_H
#define SDL1_MESH_H

#include "Vector.h"
#include "Int3.h"


class Mesh {
public:
    ~Mesh() {
        delete verticies;
        delete triangles;
    }

    Vector *verticies = nullptr;
    Int3 *triangles = nullptr;

    int numVerticies = 0;
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
