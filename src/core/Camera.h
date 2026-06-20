#ifndef SDL1_CAMERA_H
#define SDL1_CAMERA_H

#include "Vector.h"
#include <cmath>
#include <map>

#include "Matrix4D.h"

class Camera {
public:
    Camera() {
        UpdateDirectionVectors();
    }

    Vector Position = Vector(0, 0, 0);
    float pitch{}, yaw{}, roll{};

    Vector worldUp = Vector(0, 1, 0);
    Vector forward;
    Vector right;
    Vector up;

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
    // std::vector<std::pair<MoveStates, bool> > currentMoveStates;

    float fovY = 80;
    float near = 0.1f;
    float far = 100.f;

    const float DEG_2_RAD = M_PI / 180.0f;

    // float yawRad = yaw * DEG_2_RAD;
    // float pitchRad = pitch * DEG_2_RAD;
    // Vector worldUp = Vector(0, 1, 0);
    // Vector forward = Vector(cos(pitchRad) * cos(yawRad), sin(pitchRad), cos(pitchRad) * sin(yawRad)).Normalized();
    // Vector right = worldUp.Cross(forward).Normalized();
    // Vector up = forward.Cross(right).Normalized();
    float aspect = 1;

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

    Matrix4D GetViewMatrix() {
        return Matrix4D(
            right.x, right.y, right.z, -right.Dot(Position),
            up.x, up.y, up.z, -up.Dot(Position),
            -forward.x, -forward.y, -forward.z, forward.Dot(Position),
            0, 0, 0, 1
        );
    }

    Matrix4D GetProjectionMatrix() {
        float fovRad = fovY * DEG_2_RAD;
        float f = 1.0f / tan(fovRad / 2.0f);

        // Vulkan style: z in [0,1]
        return Matrix4D(
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, far / (near - far), -(far * near) / (far - near),
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
