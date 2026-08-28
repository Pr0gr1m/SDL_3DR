#ifndef SDL1_VERTEX3D_H
#define SDL1_VERTEX3D_H

#include <SDL3/SDL_pixels.h>
#include "Vector.h"

/**
 *@class Vertex3D
 *@brief Trivial class acting as a replacement for SDL_Vertex
 */
class Vertex3D {
public:
    Vector Position;
    SDL_FColor Color{0, 0, 0, 0};
    Vector Normal;
    float TexCoordU = 0.f;
    float TexCoordV = 0.f;
    Vector Tangent;
};

#endif //SDL1_VERTEX3D_H
