#include "Simulation.h"

void Simulation::RegisterObjectInScene(Object o) {
    this->objectsInScene[registerObjectIndex] = o;
    this->objectVelocities[registerObjectIndex] = Vector(0, 0);
    this->objectAccelerations[registerObjectIndex] = Vector(0, 0);

    registerObjectIndex += 1;
}

void Simulation::Update() const {
    //Gravity, drag, accelerations, velocities
    for (int i = 0; i < numObjectsInScene; i += 1) {
        this->objectsInScene[i].Position += this->objectVelocities[i];

        this->objectVelocities[i] += this->objectAccelerations[i];
        this->objectAccelerations[i] = Vector(0, 0);
    }

    //Collisions
}

void Simulation::ClearObjects() {
    delete[] objectsInScene;
    delete[] objectAccelerations;
    delete[] objectVelocities;

    objectsInScene = new Object[numObjectsInScene];
    objectVelocities = new Vector[numObjectsInScene];
    objectAccelerations = new Vector[numObjectsInScene];
    registerObjectIndex = 0;
}

// Vertex3D *Simulation::GetObjectDrawCallVerticies(Object o) const {
//     auto mesh = *o.mesh;
//
//     if (mesh.numTriangles <= 0 || mesh.triangles == nullptr) {
//         Vertex3D *verticies = new Vertex3D[mesh.numVerticies];
//
//         for (int i = 0; i < mesh.numVerticies; i += 1) {
//             Vector verticie = mesh.verticies[i];
//             Vector rotatedVerticie = o.rotationMatrix3D->Multiply(verticie);
//
//             verticies[i].Position = rotatedVerticie + o.Position;
//             verticies[i].Color = SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};
//         }
//
//         return verticies;
//     }
//
//     Vertex3D *verticies = new Vertex3D[3 * mesh.numTriangles];
//
//     for (int i = 0; i < mesh.numTriangles; i += 1) {
//         Int3 triangle = mesh.triangles[i];
//
//         auto v1 = mesh.verticies[triangle.a];
//         auto v2 = mesh.verticies[triangle.b];
//         auto v3 = mesh.verticies[triangle.c];
//
//         auto rv1 = o.rotationMatrix3D->Multiply(v1);
//         auto rv2 = o.rotationMatrix3D->Multiply(v2);
//         auto rv3 = o.rotationMatrix3D->Multiply(v3);
//
//         verticies[(i * 3) + 0].Position = rv1 + o.Position;
//         verticies[(i * 3) + 1].Position = rv2 + o.Position;
//         verticies[(i * 3) + 2].Position = rv3 + o.Position;
//
//         verticies[(i * 3) + 0].Color = SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};
//         verticies[(i * 3) + 1].Color = SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};
//         verticies[(i * 3) + 2].Color = SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};
//     }
//
//     return verticies;
// }
//
// Vertex3D *Simulation::GetObjectLineVerticies(Object o) const {
//     auto mesh = *o.mesh;
//     if (mesh.numTriangles > 0 && mesh.triangles != nullptr) {
//         auto *lineVertices = new Vertex3D[mesh.numTriangles * 6];
//
//         for (int i = 0; i < mesh.numTriangles; i++) {
//             const Int3 triangle = mesh.triangles[i];
//             const Vector transformed[3] = {
//                 o.rotationMatrix3D->Multiply(mesh.verticies[triangle.a]) + o.Position,
//                 o.rotationMatrix3D->Multiply(mesh.verticies[triangle.b]) + o.Position,
//                 o.rotationMatrix3D->Multiply(mesh.verticies[triangle.c]) + o.Position
//             };
//
//             SDL_FColor color{1.f, 1.f, 1.f, 0.25f};
//
//             const int baseIndex = i * 6;
//             lineVertices[baseIndex + 0].Position = transformed[0];
//             lineVertices[baseIndex + 0].Color = color;
//             lineVertices[baseIndex + 1].Position = transformed[1];
//             lineVertices[baseIndex + 1].Color = color;
//
//             lineVertices[baseIndex + 2].Position = transformed[1];
//             lineVertices[baseIndex + 2].Color = color;
//             lineVertices[baseIndex + 3].Position = transformed[2];
//             lineVertices[baseIndex + 3].Color = color;
//
//             lineVertices[baseIndex + 4].Position = transformed[2];
//             lineVertices[baseIndex + 4].Color = color;
//             lineVertices[baseIndex + 5].Position = transformed[0];
//             lineVertices[baseIndex + 5].Color = color;
//         }
//
//         return lineVertices;
//     }
//
//     auto *lineVertices = new Vertex3D[mesh.numVerticies * 2];
//
//     for (int i = 0; i < mesh.numVerticies; i++) {
//         int next = (i + 1) % mesh.numVerticies;
//
//         Vector a = o.rotationMatrix3D->Multiply(mesh.verticies[i]) + o.Position;
//         Vector b = o.rotationMatrix3D->Multiply(mesh.verticies[next]) + o.Position;
//
//         lineVertices[i * 2 + 0].Position = a;
//         lineVertices[i * 2 + 0].Color = SDL_FColor{1, 1, 1, 1};
//         lineVertices[i * 2 + 1].Position = b;
//         lineVertices[i * 2 + 1].Color = SDL_FColor{1, 1, 1, 1};
//     }
//
//     return lineVertices;
// }

void Simulation::ApplyForceToObject(Object o, Vector dir, float mag) const {
    int index = GetObjectIndex(o);
    ApplyForceToObject(index, dir, mag);
}

void Simulation::ApplyForceToObject(int index, Vector dir, float mag) const {
    this->objectAccelerations[index] += (dir.Normalized() * mag);
}

int Simulation::GetObjectIndex(Object o) const {
    int index = -1;

    for (int i = 0; i < numObjectsInScene; i++) {
        if (objectsInScene[i] == o) {
            index = i;
            break;
        }
    }
    return index;
}
