#ifndef SDL1_OBJECT_H
#define SDL1_OBJECT_H

#include <cmath>
#include <utility>

#include "Matrix3D.h"
#include "Mesh.h"
#include "Vector.h"
#include "Vertex3D.h"

class Object {
public:
    Object() : mesh(nullptr), Position(0, 0, 0) {
        rotationMatrix3D = {
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        };
    }

    Object(Mesh *m1, Vector pos) : mesh(m1), Position(pos) {
        float alpha = 0, beta = 0, gamma = 0;

        float cx = cos(alpha), sx = sin(alpha);
        float cy = cos(beta), sy = sin(beta);
        float cz = cos(gamma), sz = sin(gamma);

        rotationMatrix3D = {
            //Row 0 (X axis in world after rotation)
            cy * cz,
            sx * sy * cz - cx * sz,
            cx * sy * cz + sx * sz,

            //Row 1 (Y axis)
            cy * sz,
            sx * sy * sz + cx * cz,
            cx * sy * sz - sx * cz,

            //Row 2 (Z axis)
            -sy,
            sx * cy,
            cx * cy
        };
    }

    Mesh *mesh;
    Vector Position;
    
    Matrix3D rotationMatrix3D{};

    Vector acceleration{};
    Vector velocity{};

    float mass = 1;

    Object(Mesh *mesh, Vector position, Vector rotAngle) : mesh(mesh), Position(position) {
        float alpha = rotAngle.z;
        float beta = rotAngle.y;
        float gamma = rotAngle.x;

        float cx = cos(alpha), sx = sin(alpha);
        float cy = cos(beta), sy = sin(beta);
        float cz = cos(gamma), sz = sin(gamma);

        rotationMatrix3D = {
            // Row 0 (X axis in world after rotation)
            cy * cz,
            sx * sy * cz - cx * sz,
            cx * sy * cz + sx * sz,

            // Row 1 (Y axis)
            cy * sz,
            sx * sy * sz + cx * cz,
            cx * sy * sz - sx * cz,

            // Row 2 (Z axis)
            -sy,
            sx * cy,
            cx * cy
        };
    };

    // void RecalculateRotationMatrix() {
    //     float alpha = rotAngle.z;
    //     float beta = rotAngle.y;
    //     float gamma = rotAngle.x;
    //
    //     float cx = cos(alpha), sx = sin(alpha);
    //     float cy = cos(beta), sy = sin(beta);
    //     float cz = cos(gamma), sz = sin(gamma);
    //
    //     delete rotationMatrix3D;
    //     rotationMatrix3D = new Matrix3D(
    //         // Row 0 (X axis in world after rotation)
    //         cy * cz,
    //         sx * sy * cz - cx * sz,
    //         cx * sy * cz + sx * sz,
    //         // Row 1 (Y axis)
    //         cy * sz,
    //         sx * sy * sz + cx * cz,
    //         cx * sy * sz - sx * cz,
    //
    //         // Row 2 (Z axis)
    //         -sy,
    //         sx * cy,
    //         cx * cy
    //     );
    // }

    Vertex3D *GetObjectMeshDrawCallVerticies() const {
        auto chooseFaceTangent = [](const Vector &normal) {
            const float absX = std::fabs(normal.x);
            const float absY = std::fabs(normal.y);
            const float absZ = std::fabs(normal.z);

            if (absY >= absX && absY >= absZ) {
                return Vector(1.f, 0.f, 0.f);
            }

            if (absX >= absZ) {
                return Vector(0.f, 0.f, 1.f);
            }

            return Vector(1.f, 0.f, 0.f);
        };

        auto chooseFaceUv = [](const Vector &point, const Vector &normal) {
            const float absX = std::fabs(normal.x); //abs for float vars
            const float absY = std::fabs(normal.y);
            const float absZ = std::fabs(normal.z);

            float UVOffset = 0.f;
            if (absY >= absX && absY >= absZ) {
                return std::pair<float, float>{point.x + UVOffset, point.z + UVOffset};
            }

            if (absX >= absZ) {
                return std::pair<float, float>{point.z + UVOffset, point.y + UVOffset};
            }

            return std::pair<float, float>{point.x + UVOffset, point.y + UVOffset};
        };

        if (mesh->numTriangles <= 0 || mesh->triangles == nullptr) {
            Vertex3D *verticies = new Vertex3D[mesh->numVerticies];

            for (int i = 0; i < mesh->numVerticies; i += 1) {
                Vector verticie = mesh->verticies[i];
                Vector rotatedVerticie = rotationMatrix3D.Multiply(verticie);

                verticies[i].Position = rotatedVerticie + Position;
                verticies[i].Color = SDL_FColor{.0f, .0f, .0f, 1.0f};
                verticies[i].Normal = Vector(0.f, 1.f, 0.f);
                verticies[i].Tangent = Vector(1.f, 0.f, 0.f);
                verticies[i].TexCoordU = verticie.x + 0.5f;
                verticies[i].TexCoordV = verticie.z + 0.5f;
            }

            return verticies;
        }

        Vertex3D *verticies = new Vertex3D[3 * mesh->numTriangles];

        for (int i = 0; i < mesh->numTriangles; i += 1) {
            Int3 triangle = mesh->triangles[i];

            auto v1 = mesh->verticies[triangle.a];
            auto v2 = mesh->verticies[triangle.b];
            auto v3 = mesh->verticies[triangle.c];

            auto rv1 = rotationMatrix3D.Multiply(v1);
            auto rv2 = rotationMatrix3D.Multiply(v2);
            auto rv3 = rotationMatrix3D.Multiply(v3);
            auto localFaceNormal = (v2 - v1).Cross(v3 - v1).Normalized();

            const Vector localFaceCenter = (v1 + v2 + v3) / 3.f;
            if (localFaceNormal.Dot(localFaceCenter) < 0.f) {
                localFaceNormal = localFaceNormal * -1.f;
            }

            auto faceNormal = (rv2 - rv1).Cross(rv3 - rv1).Normalized();

            const Vector faceCenter = (rv1 + rv2 + rv3) / 3.f;
            if (faceNormal.Dot(faceCenter) < 0.f) {
                faceNormal = faceNormal * -1.f;
            }

            auto faceTangent = rotationMatrix3D.Multiply(chooseFaceTangent(localFaceNormal)).Normalized();
            auto uv1 = chooseFaceUv(v1, localFaceNormal);
            auto uv2 = chooseFaceUv(v2, localFaceNormal);
            auto uv3 = chooseFaceUv(v3, localFaceNormal);

            verticies[(i * 3) + 0].Position = rv1 + Position;
            verticies[(i * 3) + 1].Position = rv2 + Position;
            verticies[(i * 3) + 2].Position = rv3 + Position;

            SDL_FColor defaultColor = {.5f, .5f, .5f, 1.f};
            verticies[(i * 3) + 0].Color = defaultColor; //SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};
            verticies[(i * 3) + 1].Color = defaultColor; //SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};
            verticies[(i * 3) + 2].Color = defaultColor; //SDL_FColor{1.0f, 0.0f, 0.0f, 1.0f};

            verticies[(i * 3) + 0].Normal = faceNormal;
            verticies[(i * 3) + 1].Normal = faceNormal;
            verticies[(i * 3) + 2].Normal = faceNormal;

            verticies[(i * 3) + 0].Tangent = faceTangent;
            verticies[(i * 3) + 1].Tangent = faceTangent;
            verticies[(i * 3) + 2].Tangent = faceTangent;

            verticies[(i * 3) + 0].TexCoordU = uv1.first;
            verticies[(i * 3) + 0].TexCoordV = uv1.second;
            verticies[(i * 3) + 1].TexCoordU = uv2.first;
            verticies[(i * 3) + 1].TexCoordV = uv2.second;
            verticies[(i * 3) + 2].TexCoordU = uv3.first;
            verticies[(i * 3) + 2].TexCoordV = uv3.second;
        }

        return verticies;
    }

    Vertex3D *GetObjectLineVerticies() const {
        if (mesh->numTriangles > 0 && mesh->triangles != nullptr) {
            auto *lineVertices = new Vertex3D[mesh->numTriangles * 6];

            for (int i = 0; i < mesh->numTriangles; i++) {
                const Int3 triangle = mesh->triangles[i];
                const Vector transformed[3] = {
                    rotationMatrix3D.Multiply(mesh->verticies[triangle.a]) + Position,
                    rotationMatrix3D.Multiply(mesh->verticies[triangle.b]) + Position,
                    rotationMatrix3D.Multiply(mesh->verticies[triangle.c]) + Position
                };

                SDL_FColor color{1.f, 1.f, 1.f, .225f};

                const int baseIndex = i * 6;
                lineVertices[baseIndex + 0].Position = transformed[0];
                lineVertices[baseIndex + 0].Color = color;
                lineVertices[baseIndex + 1].Position = transformed[1];
                lineVertices[baseIndex + 1].Color = color;

                lineVertices[baseIndex + 2].Position = transformed[1];
                lineVertices[baseIndex + 2].Color = color;
                lineVertices[baseIndex + 3].Position = transformed[2];
                lineVertices[baseIndex + 3].Color = color;

                lineVertices[baseIndex + 4].Position = transformed[2];
                lineVertices[baseIndex + 4].Color = color;
                lineVertices[baseIndex + 5].Position = transformed[0];
                lineVertices[baseIndex + 5].Color = color;

                for (int j = 0; j < 6; ++j) {
                    lineVertices[baseIndex + j].Normal = Vector(0.f, 1.f, 0.f);
                    lineVertices[baseIndex + j].Tangent = Vector(1.f, 0.f, 0.f);
                    lineVertices[baseIndex + j].TexCoordU = 0.f;
                    lineVertices[baseIndex + j].TexCoordV = 0.f;
                }
            }

            return lineVertices;
        }

        auto *lineVertices = new Vertex3D[mesh->numVerticies * 2];

        for (int i = 0; i < mesh->numVerticies; i++) {
            int next = (i + 1) % mesh->numVerticies;

            Vector a = rotationMatrix3D.Multiply(mesh->verticies[i]) + Position;
            Vector b = rotationMatrix3D.Multiply(mesh->verticies[next]) + Position;

            lineVertices[i * 2 + 0].Position = a;
            lineVertices[i * 2 + 0].Color = SDL_FColor{1, 1, 1, 1};
            lineVertices[i * 2 + 0].Normal = Vector(0.f, 1.f, 0.f);
            lineVertices[i * 2 + 0].Tangent = Vector(1.f, 0.f, 0.f);
            lineVertices[i * 2 + 1].Position = b;
            lineVertices[i * 2 + 1].Color = SDL_FColor{1, 1, 1, 1};
            lineVertices[i * 2 + 1].Normal = Vector(0.f, 1.f, 0.f);
            lineVertices[i * 2 + 1].Tangent = Vector(1.f, 0.f, 0.f);
        }

        return lineVertices;
    }

    void AddForceAcceleration(Vector force);

    bool operator==(const Object &object) const {
        return mesh == object.mesh && Position == object.Position;
    }
};

inline void Object::AddForceAcceleration(Vector force) {
    this->acceleration += (force / mass);
}


#endif //SDL1_OBJECT_H

