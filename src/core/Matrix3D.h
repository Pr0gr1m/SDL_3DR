#ifndef SDL1_MATRIX2D_H
#define SDL1_MATRIX2D_H

#include "Vector.h"

/* STRUCTURE OF M2D:
x0 x1
y0 y1
*/

class Matrix2D {
public:
    float x0, y0;
    float x1, y1;

    Matrix2D(float x1, float y1, float x2, float y2) : x0(x1), y0(y1), x1(x2), y1(y2) {
    }

    Vector Multiply(Vector src) {
        float xpi = src.x * x0 + (src.y * x1);
        float ypi = src.x * y0 + (src.y * y1);

        return Vector(ypi, xpi);
    }

    // Int2 Test(Vector src, float angle) {
    //     int xpi = src.a * cos(angle) - (src.b * sin(angle));
    //     int ypi = src.b * sin(angle) + (src.a * cos(angle));
    //
    //     std::cout << src.a << " " << cos(angle) << std::endl;
    //
    //     return Int2(xpi, ypi);
    // }
};


#endif //SDL1_MATRIX2D_H
