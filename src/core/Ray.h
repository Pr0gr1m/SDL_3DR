#ifndef SDL1_RAY_H
#define SDL1_RAY_H
#include "Vector.h"

/**
 *@struct Ray
 *@brief A trivial struct representing a ray, containing a vector for position and direction
 */
struct Ray {
    ///Ray's position / starting point
    Vector position;
    ///Ray's dirrection
    Vector direction;
};

/**
 *@struct RaycastHit
 *@brief Trivial struct representing a hit from a raycast
 */
struct RaycastHit {
    ///Did the ray hit anything
    bool hit = false;

    ///Block type that was hit (boolean as there is 1 block type, mentioned in implementation of App::ConstructChunkAt)
    bool blockType = false;

    ///Hit block's world position
    Vector blockPosition{};
};

// #ifndef RaycastHit_NULL_H
// #define RaycastHit_NULL_H RaycastHit_NULL
// inline extern const RaycastHit RaycastHit_NULL{};
// #endif

#endif //SDL1_RAY_H
