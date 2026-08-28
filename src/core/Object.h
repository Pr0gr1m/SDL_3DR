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
    Object() : mesh(nullptr), Position(0, 0, 0) {
        rotationMatrix3D = {
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        };
    }

    Object(Mesh *m1, Vector pos) : mesh(m1), Position(pos) {
        float alpha = 0, beta = 0, gamma = 0;

        float cx = cos(alpha), sx = sin(alpha);
        float cy = cos(beta), sy = sin(beta);
        float cz = cos(gamma), sz = sin(gamma);

        rotationMatrix3D = {
            //Row 0 (X axis in world after rotation)
            cy * cz,
            sx * sy * cz - cx * sz,
            cx * sy * cz + sx * sz,

            //Row 1 (Y axis)
            cy * sz,
            sx * sy * sz + cx * cz,
            cx * sy * sz - sx * cz,

            //Row 2 (Z axis)
            -sy,
            sx * cy,
            cx * cy
        };
    }

    Object(Mesh *mesh, Vector position, Vector rotAngle) : mesh(mesh), Position(position) {
        float alpha = rotAngle.z;
        float beta = rotAngle.y;
        float gamma = rotAngle.x;

        float cx = cos(alpha), sx = sin(alpha);
        float cy = cos(beta), sy = sin(beta);
        float cz = cos(gamma), sz = sin(gamma);

        rotationMatrix3D = {
            // Row 0 (X axis in world after rotation)
            cy * cz,
            sx * sy * cz - cx * sz,
            cx * sy * cz + sx * sz,

            // Row 1 (Y axis)
            cy * sz,
            sx * sy * sz + cx * cz,
            cx * sy * sz - sx * cz,

            // Row 2 (Z axis)
            -sy,
            sx * cy,
            cx * cy
        };
    };

    ///Pointer to object's mesh
    Mesh *mesh;
    ///Objects world position
    Vector Position;

    ///Objects rotation matrix
    Matrix3D rotationMatrix3D{};

    ///Objects acceleration
    Vector acceleration{};
    ///Objects velocity
    Vector velocity{};

    ///Objects mass
    float mass = 1;

    /**
     *Adds force divided by mass to acceleration
     *@param force Force vector
    */
    void AddForceAcceleration(Vector force);

    bool operator==(const Object &object) const {
        return mesh == object.mesh && Position == object.Position;
    }
};

inline void Object::AddForceAcceleration(Vector force) {
    this->acceleration += (force / mass);
}


#endif //SDL1_OBJECT_H

