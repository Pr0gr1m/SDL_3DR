#ifndef SDL1_SIMULATION_H
#define SDL1_SIMULATION_H

#include <SDL3/SDL_render.h>
#include "Object.h"
#include "Vertex3D.h"

class Simulation {
public :
    Simulation() = default;

    Simulation(int numObjectsInScene) : numObjectsInScene(numObjectsInScene) {
        objectsInScene = new Object[numObjectsInScene];
        objectVelocities = new Vector[numObjectsInScene];
        objectAccelerations = new Vector[numObjectsInScene];
        registerObjectIndex = 0;
    };

    ~Simulation() {
        delete[] objectsInScene;
        delete[] objectVelocities;
        delete[] objectAccelerations;
    }

    int registerObjectIndex = 0;

    void RegisterObjectInScene(Object);

    void Update() const;

    void ClearObjects();

    // Vertex3D *GetObjectDrawCallVerticies(Object) const;
    //
    // Vertex3D *GetObjectLineVerticies(Object) const;
    //     auto mesh = o.mesh;
    //     auto *lineVertices = new Vertex3D[mesh.numVerticies * 2];
    //
    //     for (int i = 0; i < mesh.numVerticies; i++) {
    //         int next = (i + 1) % mesh.numVerticies;
    //
    //         Vector a = o.rotationMatrix2D.Multiply(mesh.verticies[i]) + o.Position;
    //         Vector b = o.rotationMatrix2D.Multiply(mesh.verticies[next]) + o.Position;
    //
    //         lineVertices[i * 2 + 0].Position = a;
    //         lineVertices[i * 2 + 0].Color = SDL_FColor{1, 1, 1, 1};
    //
    //         lineVertices[i * 2 + 1].Position = b;
    //         lineVertices[i * 2 + 1].Color = SDL_FColor{1, 1, 1, 1};
    //     }
    //
    //     return lineVertices;
    // }

    void ApplyForceToObject(Object, Vector, float) const;

    void ApplyForceToObject(int, Vector, float) const;

    int GetObjectIndex(Object) const;

    int numObjectsInScene = 0;
    Object *objectsInScene;
    Vector *objectVelocities;
    Vector *objectAccelerations;
};

#endif //SDL1_SIMULATION_H
