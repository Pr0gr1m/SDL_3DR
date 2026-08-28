#ifndef SDL1_INT2_H
#define SDL1_INT2_H

#include "Vector.h"

/**
 * @class Int3
 * @brief A class containing 3 integer fields
 */
class Int3 {
public:
    int a, b, c;

    Int3() : a(0), b(0), c(0) {
    }

    Int3(int a, int b, int c) : a(a), b(b), c(c) {
    }

    /**
     *Returns a vector containing same fields as this Int3 instance
     *@returns A vector
     */
    Vector toVector() const;
};


#endif //SDL1_INT2_H
