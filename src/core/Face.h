#ifndef SDL1_FACE_H
#define SDL1_FACE_H
#include <array>

#include "Matrix3D.h"
#include "Vertex3D.h"

/**
 *@struct Face
 *@brief Struct representing single mesh face
*/
struct Face {
    ///Face's points world positions
    Vector globalPoint1, globalPoint2, globalPoint3{};
    ///Rotation matrix
    Matrix3D rotationMatrix3D{Matrix3D::Identity()};

    Face() = default;

    Face(Vector globalPoint1, Vector globalPoint2, Vector globalPoint3) : globalPoint1(globalPoint1), globalPoint2(globalPoint2), globalPoint3(globalPoint3) {
    }

    Face(Matrix3D rotationMatrix, Vector globalPoint1, Vector globalPoint2, Vector globalPoint3) : rotationMatrix3D(rotationMatrix), globalPoint1(globalPoint1), globalPoint2(globalPoint2), globalPoint3(globalPoint3) {
    }

    ///Returns an array containing 3 Vertex3D objects, ready to be rendered
    [[nodiscard]] std::array<Vertex3D, 3> GetFaceDrawCallVerticies() const;

    ///Returns an array containing 6 Vertex3D objects, representing the face outline, ready to be rendered
    [[nodiscard]] std::array<Vertex3D, 6> GetFaceLineCallVerticies() const;
    
    Face &operator+=(const Vector &vector) {
        globalPoint1 += vector;
        globalPoint2 += vector;
        globalPoint3 += vector;

        return *this;
    }
};

#endif //SDL1_FACE_H
