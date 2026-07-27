#ifndef SDL1_CAMERA_H
#define SDL1_CAMERA_H

#include <array>

#include "Vector.h"
#include <cmath>
#include <map>

#include "Matrix4D.h"
#include "Object.h"

class Camera {
public:
    Camera() {
        UpdateDirectionVectors();
    }

    ~Camera() {
    }

    Vector Position = Vector(0, 0, 0);
    float pitch{}, yaw{}, roll{};

    Vector worldUp = Vector(0, 1, 0);
    Vector forward; //Default : (0,0,-1)
    Vector right;
    Vector up;

    // Vector frustrumPlanes[6];
    // Vector frustrumCorners[8];

    //or plane normal * any point + distance/offset from origin = 0
    //Ax + By + Cz + D = 0
    struct FrustrumPlane {
        // Vector planeNormal;
        // float originDistance;
        //
        // float DistanceToPoint(const Vector &p) const {
        //     return planeNormal.Dot(p) + originDistance;
        // }
        float A;
        float B;
        float C;
        float D;
    };

    std::array<FrustrumPlane, 6> frustrumPlanes;
    std::array<Vector, 8> frustrumCorners;

    Camera(float p, float y, float r) : pitch(p), yaw(y), roll(r) {
        UpdateDirectionVectors();
    }

    enum MoveStates {
        Forward,
        Backward,
        Left,
        Right,
        Up,
        Down,
        None //Should be last, makes it possible to know num of elements static_cast<int>(Example::None)
    };

    std::map<MoveStates, bool> currentMoveStates;

    float fovY = 80;
    float near = 0.1f;
    float far = 100.f;

    const float DEG_2_RAD = M_PI / 180.0f;

    float aspect = 1;
    Uint64 deltaTimeMS{};

    Camera(Vector positon, float pitch, float yaw, float roll) : Position(positon), pitch(pitch), yaw(yaw), roll(roll) {
        //currentMoveStates = std::vector<std::pair<MoveStates, bool> >(static_cast<int>(MoveStates::None));
        currentMoveStates = std::map<MoveStates, bool>();
        UpdateDirectionVectors();
    };

    Camera(Vector positon, float pitch, float yaw, float roll, float aspect) : Position(positon), pitch(pitch), yaw(yaw), roll(roll), aspect(aspect) {
        currentMoveStates = std::map<MoveStates, bool>();
        UpdateDirectionVectors();
    };

    Vector cameraVelocity = Vector(0, 0, 0);
    double cameraMass = 10.00;

    void SetMoveState(MoveStates, bool);

    bool GetMoveState(MoveStates);

    void AddForceThisTick(Vector, float);

    void AddForceThisTick(Vector);

    void MoveCameraBasedOnVelocity();

    ///Returns 8 vector points
    void UpdateCameraFrustrumCorners();

    bool IsPointInFrustum(const Vector &point) const;

    Matrix4D GetViewMatrix() const {
        return Matrix4D(
            right.x, right.y, right.z, -Position.Dot(right),
            up.x, up.y, up.z, -Position.Dot(up),
            -forward.x, -forward.y, -forward.z, Position.Dot(forward),
            0, 0, 0, 1
        );
    }

    Matrix4D GetProjectionMatrix() const {
        float fovRad = fovY * DEG_2_RAD;
        float f = 1.0f / tan(fovRad / 2.0f);
        
        float safeAspect = (aspect == 0.f) ? 1.f : aspect;
        float zRange = near - far;
        if (zRange == 0.f) zRange = -0.001f;

        // Vulkan style: z in [0,1]
        return Matrix4D(
            f / safeAspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, far / zRange, -(far * near) / (far - near == 0.f ? 0.001f : far - near),
            0, 0, -1, 0
        );
    }

    void UpdateDirectionVectors() {
        float yawRad = yaw * DEG_2_RAD;
        float pitchRad = pitch * DEG_2_RAD;

        forward = Vector(cos(pitchRad) * sin(yawRad), sin(pitchRad), -cos(pitchRad) * cos(yawRad)).Normalized();
        right = forward.Cross(worldUp).Normalized();
        up = right.Cross(forward).Normalized();
    }

    void ResetVelocityAlongWorldAxis(Vector);
};


#endif //SDL1_CAMERA_H
