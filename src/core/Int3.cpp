#include "Int2.h"
#include "Vector.h"

Vector Int2::toVector() const {
    return Vector(static_cast<float>(a), static_cast<float>(b));
}
