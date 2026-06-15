#ifndef SDL1_MATRIX2D_H
#define SDL1_MATRIX2D_H

#include "Vector.h"

/* STRUCTURE OF M2D:
x0 x1
y0 y1
*/

class Matrix3D {
public:
    float x0, y0, z0;
    float x1, y1, z1;
    float x2, y2, z2;

    Matrix3D(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2) : x0(x0), y0(y0), z0(z0), x1(x1), y1(y1), z1(z1), x2(x2), y2(y2), z2(z2) {
    }

    Vector Multiply(Vector src) const {
        float a = (x0 * src.x) + (y0 * src.y) + (z0 * src.z);
        float b = (x1 * src.x) + (y1 * src.y) + (z1 * src.z);
        float c = (x2 * src.x) + (y2 * src.y) + (z2 * src.z);

        return Vector(a, b, c);
    }

    static Matrix3D Identity();

    // Vector Multiply(Vector src) {
    //     float xpi = src.x * x0 + (src.y * x1);
    //     float ypi = src.x * y0 + (src.y * y1);
    //
    //     return Vector(ypi, xpi);
    // }

    // Int2 Test(Vector src, float angle) {
    //     int xpi = src.a * cos(angle) - (src.b * sin(angle));
    //     int ypi = src.b * sin(angle) + (src.a * cos(angle));
    //
    //     std::cout << src.a << " " << cos(angle) << std::endl;
    //
    //     return Int2(xpi, ypi);
    // }
};

inline Matrix3D Matrix3D::Identity() {
    return Matrix3D(1, 0, 0, 0, 1, 0, 0, 0, 1);
}


#endif //SDL1_MATRIX2D_H
