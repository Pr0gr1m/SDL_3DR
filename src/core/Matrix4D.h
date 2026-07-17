#ifndef SDL1_MATRIX4D_H
#define SDL1_MATRIX4D_H

#define F2STRING(Value) #Value

#include <string>

/* STRUCTURE OF M2D:
x0 x1 x2 x3
y0 y1 y2 y3
z0 z1 z2 z3
j0 j1 j2 j3
*/

class Matrix4D {
public:
    float x0{}, y0{}, z0{}, j0{};
    float x1{}, y1{}, z1{}, j1{};
    float x2{}, y2{}, z2{}, j2{};
    float x3{}, y3{}, z3{}, j3{};

    Matrix4D() = default;

    Matrix4D(float x0, float y0, float z0, float j0, float x1, float y1, float z1, float j1, float x2, float y2, float z2, float j2, float x3, float y3, float z3, float j3)
        : x0(x0), y0(y0), z0(z0), j0(j0), x1(x1), y1(y1), z1(z1), j1(j1), x2(x2), y2(y2), z2(z2), j2(j2), x3(x3), y3(y3), z3(z3), j3(j3) {
    }
    
    void toOutFloat16Array(float out[16]) const {
        out[0] = x0;
        out[1] = y0;
        out[2] = z0;
        out[3] = j0;
        out[4] = x1;
        out[5] = y1;
        out[6] = z1;
        out[7] = j1;
        out[8] = x2;
        out[9] = y2;
        out[10] = z2;
        out[11] = j2;
        out[12] = x3;
        out[13] = y3;
        out[14] = z3;
        out[15] = j3;
    }

    Matrix4D operator*(const Matrix4D &other) const {
        return Matrix4D(
            // Row 0
            x0 * other.x0 + y0 * other.x1 + z0 * other.x2 + j0 * other.x3,
            x0 * other.y0 + y0 * other.y1 + z0 * other.y2 + j0 * other.y3,
            x0 * other.z0 + y0 * other.z1 + z0 * other.z2 + j0 * other.z3,
            x0 * other.j0 + y0 * other.j1 + z0 * other.j2 + j0 * other.j3,

            // Row 1
            x1 * other.x0 + y1 * other.x1 + z1 * other.x2 + j1 * other.x3,
            x1 * other.y0 + y1 * other.y1 + z1 * other.y2 + j1 * other.y3,
            x1 * other.z0 + y1 * other.z1 + z1 * other.z2 + j1 * other.z3,
            x1 * other.j0 + y1 * other.j1 + z1 * other.j2 + j1 * other.j3,

            // Row 2
            x2 * other.x0 + y2 * other.x1 + z2 * other.x2 + j2 * other.x3,
            x2 * other.y0 + y2 * other.y1 + z2 * other.y2 + j2 * other.y3,
            x2 * other.z0 + y2 * other.z1 + z2 * other.z2 + j2 * other.z3,
            x2 * other.j0 + y2 * other.j1 + z2 * other.j2 + j2 * other.j3,

            // Row 3
            x3 * other.x0 + y3 * other.x1 + z3 * other.x2 + j3 * other.x3,
            x3 * other.y0 + y3 * other.y1 + z3 * other.y2 + j3 * other.y3,
            x3 * other.z0 + y3 * other.z1 + z3 * other.z2 + j3 * other.z3,
            x3 * other.j0 + y3 * other.j1 + z3 * other.j2 + j3 * other.j3
        );
    }

    const char *toString() const {
        return (std::string(F2STRING(x0)) + "," + std::string(F2STRING(x1)) + std::string(F2STRING(x2)) + "," + std::string(F2STRING(x3)) + "\n" +
                std::string(F2STRING(y0)) + "," + std::string(F2STRING(y1)) + std::string(F2STRING(y2)) + "," + std::string(F2STRING(y3)) + "\n" +
                std::string(F2STRING(z0)) + "," + std::string(F2STRING(z1)) + std::string(F2STRING(z2)) + "," + std::string(F2STRING(z3)) + "\n" +
                std::string(F2STRING(j0)) + "," + std::string(F2STRING(j1)) + std::string(F2STRING(j2)) + "," + std::string(F2STRING(j3)) + "\n").c_str();
    }
};

#endif //SDL1_MATRIX4D_H
