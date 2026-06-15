#include "Int3.h"
#include "Vector.h"

Vector Int3::toVector() const {
    return Vector(static_cast<float>(a), static_cast<float>(b), static_cast<float>(c));
}
