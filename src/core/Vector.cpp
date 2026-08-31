#include "Vector.h"

#include <cmath>
#include "Int3.h"

Int3 Vector::toInt3() const {
    return Int3(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
}

Vector Vector::floored() const {
    return Vector(std::floor(x), std::floor(y), std::floor(z));
}

