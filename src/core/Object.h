#ifndef SDL1_OBJECT_H
#define SDL1_OBJECT_H

#include <cmath>

#include "Matrix3D.h"
#include "Mesh.h"
#include "Vector.h"

/**
 *@class Object
 *@brief Class representing a created object with a mesh e.g cube at a position
 */
class Object {
public:
    Object() : mesh(nullptr), Position(0, 0, 0), alpha(0), beta(0), gamma(0) {
    }

    Object(Mesh *m1, Vector pos) : mesh(m1), Position(pos), alpha(0), beta(0), gamma(0) {
        // float alpha = 0, beta = 0, gamma = 0;
        //
        // float cx = cos(alpha), sx = sin(alpha);
        // float cy = cos(beta), sy = sin(beta);
        // float cz = cos(gamma), sz = sin(gamma);
        //
        // rotationMatrix3D = {
        //     //Row 0 (X axis in world after rotation)
        //     cy * cz,
        //     sx * sy * cz - cx * sz,
        //     cx * sy * cz + sx * sz,
        //
        //     //Row 1 (Y axis)
        //     cy * sz,
        //     sx * sy * sz + cx * cz,
        //     cx * sy * sz - sx * cz,
        //
        //     //Row 2 (Z axis)
        //     -sy,
        //     sx * cy,
        //     cx * cy
        // };
    }

    Object(Mesh *mesh, Vector position, Vector rotAngle) : mesh(mesh), Position(position), alpha(rotAngle.x), beta(rotAngle.y), gamma(rotAngle.z) {
        // float alpha = rotAngle.z;
        // float beta = rotAngle.y;
        // float gamma = rotAngle.x;
        //
        // float cx = cos(alpha), sx = sin(alpha);
        // float cy = cos(beta), sy = sin(beta);
        // float cz = cos(gamma), sz = sin(gamma);
        // rotationMatrix3D = {
        //     // Row 0 (X axis in world after rotation)
        //     cy * cz,
        //     sx * sy * cz - cx * sz,
        //     cx * sy * cz + sx * sz,
        //
        //     // Row 1 (Y axis)
        //     cy * sz,
        //     sx * sy * sz + cx * cz,
        //     cx * sy * sz - sx * cz,
        //
        //     // Row 2 (Z axis)
        //     -sy,
        //     sx * cy,
        //     cx * cy
        // };
    };

    ///Pointer to object's mesh
    Mesh *mesh;

    ///Objects world position
    Vector Position;

    ///Objects rotation matrix
    float alpha, beta, gamma;
};


#endif //SDL1_OBJECT_H

