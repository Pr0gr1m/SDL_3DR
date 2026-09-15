#include "Vector.h"

#include <cmath>
#include "Int3.h"

Int3 Vector::toInt3() const {
    return Int3(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::floor(z)));
}

Vector Vector::floored() const {
    return Vector(std::floor(x), std::floor(y), std::floor(z));
}

