#ifndef SDL1_INT2_H
#define SDL1_INT2_H

#include <SDL3/SDL_render.h>

class Vector;

class Int2 {
public:
    int a, b;

    Int2() : a(0), b(0) {
    }

    Int2(int a, int b) : a(a), b(b) {
    }

    Vector toVector() const; //{ return Vector(a, b); }

    SDL_Vertex toSDL_Vertex() {
        SDL_Vertex verticie;
        SDL_FPoint fPoint = SDL_FPoint(a, b);
        verticie.position = fPoint;
        verticie.color = SDL_FColor(255, 0, 0);
        return verticie;
    }
};

// inline Int2 Vector::toInt2() const {
//     return Int2(static_cast<int>(x), static_cast<int>(y));
// }

#endif //SDL1_INT2_H
