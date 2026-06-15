#ifndef SDL1_INT2_H
#define SDL1_INT2_H

#include <SDL3/SDL_render.h>

class Vector;

class Int3 {
public:
    int a, b, c;

    Int3() : a(0), b(0), c(0) {
    }

    Int3(int a, int b, int c) : a(a), b(b), c(c) {
    }

    Vector toVector() const;

    // SDL_Vertex toSDL_Vertex() {
    //     SDL_Vertex verticie;
    //     SDL_FPoint fPoint = SDL_FPoint(a, b);
    //     verticie.position = fPoint;
    //     verticie.color = SDL_FColor(255, 0, 0);
    //     return verticie;
    // }
};


#endif //SDL1_INT2_H
