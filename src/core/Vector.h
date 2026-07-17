#ifndef SDL1_VECTOR_H
#define SDL1_VECTOR_H

#include <cmath>

class Int3;

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

    Int3 toInt3() const;

    float Magnitude() const;

    Vector Normalized() const;

    Vector Negated() const;

    Vector Cross(const Vector &) const;

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

    Vector CrossProduct(const Vector &a, const Vector &b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
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

// Vector operator*(int lhs, const Vector &rhs) {
//     return Vector(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
// }

//After decleration?


#endif //SDL1_VECTOR_H
