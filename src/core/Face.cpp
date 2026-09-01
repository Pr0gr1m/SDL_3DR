#include "Face.h"

std::array<Vertex3D, 3> Face::GetFaceDrawCallVerticies() const {
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

    auto chooseFaceUV = [](const Vector &point, const Vector &normal) {
        const float absX = std::fabs(normal.x); //abs for float
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

    std::array<Vertex3D, 3> verticies = {};

    auto v1 = globalPoint1;
    auto v2 = globalPoint2;
    auto v3 = globalPoint3;

    auto rv1 = rotationMatrix3D.Multiply(v1);
    auto rv2 = rotationMatrix3D.Multiply(v2);
    auto rv3 = rotationMatrix3D.Multiply(v3);

    auto localFaceNormal = (v2 - v1).Cross(v3 - v1).Normalized();

    auto faceNormal = (rv2 - rv1).Cross(rv3 - rv1).Normalized();

    auto faceTangent = rotationMatrix3D.Multiply(chooseFaceTangent(localFaceNormal)).Normalized();
    auto uv1 = chooseFaceUV(v1, localFaceNormal);
    auto uv2 = chooseFaceUV(v2, localFaceNormal);
    auto uv3 = chooseFaceUV(v3, localFaceNormal);

    verticies[0].Position = rv1;
    verticies[1].Position = rv2;
    verticies[2].Position = rv3;

    SDL_FColor defaultColor = {.5f, .5f, .5f, 1.f};
    verticies[0].Color = defaultColor;
    verticies[1].Color = defaultColor;
    verticies[2].Color = defaultColor;

    verticies[0].Normal = faceNormal;
    verticies[1].Normal = faceNormal;
    verticies[2].Normal = faceNormal;

    verticies[0].Tangent = faceTangent;
    verticies[1].Tangent = faceTangent;
    verticies[2].Tangent = faceTangent;

    verticies[0].TexCoordU = uv1.first;
    verticies[0].TexCoordV = uv1.second;
    verticies[1].TexCoordU = uv2.first;
    verticies[1].TexCoordV = uv2.second;
    verticies[2].TexCoordU = uv3.first;
    verticies[2].TexCoordV = uv3.second;

    return verticies;
}

std::array<Vertex3D, 6> Face::GetFaceLineCallVerticies() const {
    std::array<Vertex3D, 6> lineVertices = {};

    const Vector transformed[3] = {
        rotationMatrix3D.Multiply(globalPoint1),
        rotationMatrix3D.Multiply(globalPoint2),
        rotationMatrix3D.Multiply(globalPoint3)
    };

    for (int i = 0; i < 6; i++) {
        lineVertices[i].Color = SDL_FColor{1, 1, 1, .225};
        lineVertices[i].Normal = Vector(0.f, 1.f, 0.f);
        lineVertices[i].Tangent = Vector(1.f, 0.f, 0.f);
        lineVertices[i].TexCoordU = 0.f;
        lineVertices[i].TexCoordV = 0.f;
    }

    lineVertices[0].Position = transformed[0];
    lineVertices[1].Position = transformed[0];
    lineVertices[2].Position = transformed[1];
    lineVertices[3].Position = transformed[1];
    lineVertices[4].Position = transformed[2];
    lineVertices[5].Position = transformed[2];

    return lineVertices;
}
