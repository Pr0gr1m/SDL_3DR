#ifndef SDL1_RAY_H
#define SDL1_RAY_H
#include "core/Vector.h"

struct Ray {
    Vector position;
    Vector direction;
};

struct RaycastHit {
    bool hit = false;
    bool blockType = false; //mentioned in App::ConstructChunkAt
    Vector blockPosition{};
};

#ifndef RaycastHit_NULL_H
#define RaycastHit_NULL_H RaycastHit_NULL
inline extern const RaycastHit RaycastHit_NULL{};
// extern RaycastHit RaycastHit_NULL{false, false, Vector(0, 0, 0)};
#endif

#endif //SDL1_RAY_H
