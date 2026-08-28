#ifndef SDL1_VECTOR_H
#define SDL1_VECTOR_H

#include <cmath>

class Int3;

/**
 *@class Vector
 *@brief Trivial vector class containing 3 floats
 */
class Vector {
public:
    Vector() = default;

    float x = 0, y = 0, z = 0;

    Vector(float x) : x(x), y(0), z(0) {
    }

    Vector(float x, float y) : x(x), y(y), z(0) {
    }

    Vector(float x, float y, float z) : x(x), y(y), z(z) {
    }

    ///Returns an Int3 containing Vector's fields as integers
    Int3 toInt3() const;

    ///Returns magnitude of the vector
    float Magnitude() const;

    ///Returns new vector with the same fields divided by magnitude
    Vector Normalized() const;

    ///Negates every field
    Vector Negated() const;

    ///Returns a cross vector from this and the parameter vector
    Vector Cross(const Vector &) const;

    ///Returns a dot product from this and the parameter vector
    float Dot(const Vector &) const;

    bool operator==(const Vector &vector) const {
        return x == vector.x && y == vector.y && z == vector.z;
    }

    Vector &operator+=(const Vector &vector) {
        x += vector.x;
        y += vector.y;
        z += vector.z;
        return *this;
    }

    Vector operator*(float a) const {
        return Vector(x * a, y * a, z * a);
    }

    Vector operator*(int a) const {
        return Vector(x * a, y * a, z * a);
    }

    Vector operator*(Vector a) const {
        return Vector(x * a.x, y * a.y, z * a.z);
    }

    Vector operator/(float a) const {
        if (a == 0) {
            return Vector(0, 0, 0);
        }
        return Vector(x / a, y / a, z / a);
    }

    Vector operator+(const Vector &other) const {
        return Vector(x + other.x, y + other.y, z + other.z);
    }

    bool operator<(const Vector &other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }

    Vector operator-(const Vector &vector) const {
        return Vector(x - vector.x, y - vector.y, z - vector.z);
    }

    Vector &operator*=(const Vector &vector) {
        this->x *= vector.x;
        this->y *= vector.y;
        this->z *= vector.z;
        return *this;
    }
};

inline float Vector::Magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
}

inline Vector Vector::Normalized() const {
    const float magnitude = Magnitude();
    if (magnitude == 0.f) {
        return Vector(0, 0, 0);
    }

    return Vector(x, y, z) / magnitude;
}

inline Vector Vector::Negated() const {
    return Vector(-x, -y, -z);
}

inline Vector Vector::Cross(const Vector &other) const {
    return Vector(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

inline float Vector::Dot(const Vector &other) const {
    return x * other.x + y * other.y + z * other.z;
}

#endif //SDL1_VECTOR_H
